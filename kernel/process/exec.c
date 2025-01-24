#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <kernel/console.h>
#include <kernel/object_pool.h>
#include <kernel/elf.h>
#include <kernel/fs/fs.h>
#include <kernel/fd.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/types.h>
#include <kernel/core/irq.h>

#include "process_private.h"

#define STACK_BOTTOM  (VIRT_USTACK_TOP - USTACK_SIZE)
#define STACK_PROT    (PROT_READ | PROT_WRITE | VM_USER)

static int
user_stack_put(struct VMSpace *vm, const void *buf, size_t n, uintptr_t *va_p)
{
  uintptr_t va = ROUND_DOWN(*va_p - n, sizeof(uintptr_t));
  int r;

  if (va < STACK_BOTTOM)
    return -E2BIG;

  if ((r = vm_copy_out(vm->pgtab, buf, va, n)) < 0)
    return r;

  *va_p = va;

  return 0;
}

static int
user_stack_put_string(struct VMSpace *vm, const char *s, uintptr_t *va_p)
{
  return user_stack_put(vm, s, strlen(s) + 1, va_p);
}

#define VEC_MAX 31

static int
user_stack_put_strings(struct VMSpace *vm, char *const args[],
                       uintptr_t *args_va, uintptr_t *va_p, int *argc_p)
{
  int i, r;
  uintptr_t va;

  for (va = *va_p, i = 0; args[i] != NULL; i++) {
    if (i > VEC_MAX)
      return -E2BIG;

    if ((r = user_stack_put_string(vm, args[i], &va)))
      return r;

    args_va[i] = va;
  }
  args_va[i] = (uintptr_t) NULL;

  *va_p = va;
  *argc_p = i;

  return 0;
}

static int
user_stack_put_vector(struct VMSpace *vm, uintptr_t *vec, int count,
                      uintptr_t *va_p)
{
  return user_stack_put(vm, vec, (count + 1) * sizeof(uintptr_t), va_p);
}

struct ExecContext {
  struct Inode   *inode;
  struct VMSpace *vm;
  uintptr_t       argv[VEC_MAX + 1];
  int             argc;
  uintptr_t       envp[VEC_MAX + 1];
  int             envc;
  uintptr_t       argv_va;
  uintptr_t       env_va;
  uintptr_t       sp_va;
  uintptr_t       entry_va;
};

int
user_stack_init(char *const argv[], char *const envp[], struct ExecContext *e)
{
  uintptr_t va;
  int r;

  va = vmspace_map(e->vm, STACK_BOTTOM, USTACK_SIZE, STACK_PROT);
  if (va != STACK_BOTTOM)
    return (int) va;

  // Copy args and environment.
  va = VIRT_USTACK_TOP;

  // Copy the environment strings
  if ((r = user_stack_put_strings(e->vm, envp, e->envp, &va, &e->envc)) != 0)
    return r;
  
  // Copy the initial argument strings (additional arguments may be put later)
  if ((r = user_stack_put_strings(e->vm, argv, e->argv, &va, &e->argc)) != 0)
    return r;

  e->sp_va = va;

  return 0;
}

static int
user_stack_finalize(struct ExecContext *ctx)
{
  uintptr_t usp = ctx->sp_va;
  int r;

  // Put the final environment vector
  if ((r = user_stack_put_vector(ctx->vm, ctx->envp, ctx->envc, &usp)) < 0)
    return r;
  ctx->env_va = usp;

  // Put the final arguments vector
  if ((r = user_stack_put_vector(ctx->vm, ctx->argv, ctx->argc, &usp)) < 0)
    return r;
  ctx->argv_va = usp;

  // Stack must be aligned to an 8-byte boundary in order for variadic args
  // to properly work (at least on ARM)!
  usp = ROUND_DOWN(usp, 8);
  ctx->sp_va = usp;

  return 0;
}

static int
resolve_inode(struct Inode *inode, char *p, struct ExecContext *ctx, char **pp)
{
  char buf[1024];
  off_t off;
  int i, r, start;

  if (!S_ISREG(inode->mode))
    return -ENOENT;

  if (!fs_permission(inode, FS_PERM_EXEC, 0))
    return -EPERM;

  off = 0;
  if ((r = fs_inode_read_locked(inode, (uintptr_t) buf, 1024, &off)) < 0) 
    return r;

  if ((r < 3) || (buf[0] != '#') || (buf[1] != '!')) {
    *pp = NULL;
    return 0;
  }

  // Skip whitespace
  for (start = 2; start < r; start++)
    if ((buf[start] != ' ') && (buf[start] != '\t'))
      break;

  if (start == r || buf[start] == '\n') {
    *pp = NULL;
    return 0;
  }

  for (i = start ; i < r; i++)
    if ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n'))
      break;
  buf[i] = '\0';

  if (ctx->argc > VEC_MAX)
    return -E2BIG;
  
  if ((r = user_stack_put_string(ctx->vm, p, &ctx->sp_va)) < 0)
    return r;

  for (i = ctx->argc; i > 0; i--)
    ctx->argv[i + 1] = ctx->argv[i];
  ctx->argv[1] = ctx->sp_va;
  ctx->argc++;

  if ((p = k_malloc(strlen(&buf[start]) + 1)) == NULL)
    return -ENOMEM;

  strncpy(p, &buf[start], strlen(&buf[start]) + 1);

  *pp = p;

  return 0;
}

static int
resolve(const char *path, struct ExecContext *ctx)
{
  struct Inode *inode;
  char *p = (char *) path;

  for (;;) {
    int r;
    char *interpreter;

    if ((r = fs_lookup_inode(p, 0, &inode)) < 0)
      return r;

    fs_inode_lock(inode);

    if ((r = resolve_inode(inode, p, ctx, &interpreter)) < 0) {
      fs_inode_unlock(inode);
      fs_inode_put(inode);
      return r;
    }

    if (interpreter == NULL)
      break;

    if (p != path)
      k_free(p);

    p = interpreter;

    fs_inode_unlock(inode);
    fs_inode_put(inode);
  }

  if (p != path)
    k_free(p);

  ctx->inode = inode;

  return 0;
}

static int
load_elf(struct ExecContext *ctx)
{
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  int r;
  off_t off;
  uintptr_t a;

  off = 0;
  if ((r = fs_inode_read_locked(ctx->inode, (uintptr_t) &elf, sizeof(elf),
                                &off)) != sizeof(elf))
    return -EINVAL;

  if (memcmp(elf.ident, "\x7f""ELF", 4) != 0)
    return -EINVAL;

  off = elf.phoff;
  while ((size_t) off < elf.phoff + elf.phnum * sizeof(ph)) {
    if ((r = fs_inode_read_locked(ctx->inode, (uintptr_t) &ph, sizeof(ph),
                                  &off)) != sizeof(ph))
      return r;

    if (ph.type != PT_LOAD)
      continue;

    if (ph.filesz > ph.memsz)
      return -EINVAL;

    if ((ph.vaddr >= VIRT_KERNEL_BASE) || (ph.vaddr + ph.memsz > VIRT_KERNEL_BASE))
      return -EINVAL;

    a = vmspace_map(ctx->vm, ph.vaddr, ph.memsz,
                    PROT_READ | PROT_WRITE | PROT_EXEC | VM_USER);
    if (a != ph.vaddr)
      return (int) a;

    if ((r = vm_space_load_inode(ctx->vm->pgtab, (void *) ph.vaddr, ctx->inode,
                                 ph.filesz, ph.offset)) < 0)
      return r;
  }

  ctx->entry_va = elf.entry;

  return 0;
}

static void
sys_free_args(char **args)
{
  char **p;

  for (p = args; *p != NULL; p++)
    k_free(*p);

  k_free(args);
}

static int
copy_in_args(uintptr_t va, char ***store)
{
  void *pgtab = process_current()->vm->pgtab;
  char **args;
  size_t len;
  size_t total_len;
  int r;

  if ((vm_user_check_args(pgtab, va, &len, VM_READ | VM_USER)) < 0)
    return r;
  
  total_len = (len + 1) * sizeof(char *);
  if (total_len > ARG_MAX)
    return -E2BIG;

  if ((args = (char **) k_malloc(total_len)) == NULL)
    return -ENOMEM;

  memset(args, 0, total_len);

  for (size_t i = 0; i < len; i++) {
    uintptr_t str_va;
    size_t str_len;

    if ((r = vm_copy_in(pgtab, &str_va, va + (sizeof(char *)*i),
                        sizeof str_va)) < 0) {
      sys_free_args(args);
      return r;
    }

    if ((r = vm_user_check_str(pgtab, str_va, &str_len,
                               VM_READ | VM_USER)) < 0) {
      sys_free_args(args);
      return r;
    }

    total_len += str_len + 1;
    if (total_len > ARG_MAX) {
      sys_free_args(args);
      return -E2BIG;
    }

    if ((args[i] = k_malloc(str_len + 1)) == NULL) {
      sys_free_args(args);
      return -ENOMEM;
    }

    if ((vm_copy_in(pgtab, args[i], str_va, str_len + 1) != 0) ||
         (args[i][str_len] != '\0')) {
      sys_free_args(args);
      return -EFAULT;
    }
  }

  if (store) *store = args;

  return 0;
}

int
process_exec(const char *path, uintptr_t argv_va, uintptr_t envp_va)
{
  struct Process *proc;
  struct VMSpace *old_vm;
  int r;
  struct ExecContext ctx;

  char **argv, **envp;

  if ((ctx.vm = vm_space_create()) == NULL) {
    r = -ENOMEM;
    goto out1;
  }

  if ((r = copy_in_args(argv_va, &argv)) != 0)
    goto out2;
  
  if ((r = copy_in_args(envp_va, &envp)) != 0)
    goto out3;

  if ((r = user_stack_init(argv, envp, &ctx)) != 0)
    goto out4;

  if ((r = resolve(path, &ctx)) != 0)
    goto out4;

  if ((r = user_stack_finalize(&ctx)) != 0)
    goto out5;

  if ((r = load_elf(&ctx)) != 0)
    goto out5;

  fs_inode_unlock(ctx.inode);
  fs_inode_put(ctx.inode);

  sys_free_args(envp);
  sys_free_args(argv);

  proc = process_current();

  fd_close_on_exec(proc);

  strncpy(proc->name, path, 63);

  process_lock();

  old_vm = proc->vm;
  proc->vm = ctx.vm;

  arch_vm_load(ctx.vm->pgtab);

  process_unlock();

  vm_space_destroy(old_vm);

  return arch_trap_frame_init(proc, ctx.entry_va, ctx.argc,
                              ctx.argv_va, 
                              ctx.env_va,
                              ctx.sp_va);

out5:
  fs_inode_unlock(ctx.inode);
  fs_inode_put(ctx.inode);
out4:
  sys_free_args(envp);
out3:
  sys_free_args(argv);
out2:
  vm_space_destroy(ctx.vm);
out1:
  return r;
}
