#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/elf.h>
#include <kernel/fs/fs.h>
#include <kernel/fd.h>
#include <kernel/mm/memlayout.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/types.h>

static int
copy_args(struct VMSpace *vm, char *const args[], uintptr_t limit, char **sp)
{
  char *oargs[32];
  char *p;
  size_t n;
  int i, r;
  
  for (p = *sp, i = 0; args[i] != NULL; i++) {
    if (i >= 32)
      return -E2BIG;

    n = strlen(args[i]);
    p -= ROUND_UP(n + 1, sizeof(uint32_t));

    if (p < (char *) limit)
      return -E2BIG;

    if ((r = vm_copy_out(vm->pgdir, p, args[i], n)) < 0)
      return r;

    oargs[i] = p;
  }
  oargs[i] = NULL;

  n = (i + 1) * sizeof(char *);
  p -= ROUND_UP(n, sizeof(uint32_t));
  
  if (p < (char *) (VIRT_USTACK_TOP - USTACK_SIZE))
    return -E2BIG;

  if ((r = vm_copy_out(vm->pgdir, p, oargs, n)) < 0)
    return r;

  *sp = p;

  return i;
}

int
process_exec(const char *path, char *const argv[], char *const envp[])
{
  struct Process *proc;
  struct Inode *ip;
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  off_t off;
  struct VMSpace *vm;
  uintptr_t heap, ustack;
  char *usp, *uargv, *uenvp;
  int r, argc;
  intptr_t a;

  if ((r = fs_name_lookup(path, 0, &ip)) < 0)
    return r;
  if (ip == NULL)
    return -ENOENT;

  fs_inode_lock(ip);

  if (!S_ISREG(ip->mode)) {
    r = -ENOENT;
    goto out1;
  }

  if (!fs_permission(ip, FS_PERM_EXEC, 0)) {
    r = -EPERM;
    goto out1;
  }

  vm = vm_space_create();

  off = 0;
  if ((r = fs_inode_read(ip, &elf, sizeof(elf), &off)) != sizeof(elf))
    goto out2;
  
  if (memcmp(elf.ident, "\x7f""ELF", 4) != 0) {
    r = -EINVAL;
    goto out2;
  }

  heap   = 0;
  ustack = VIRT_USTACK_TOP - USTACK_SIZE;

  off = elf.phoff;
  while ((size_t) off < elf.phoff + elf.phnum * sizeof(ph)) {
    if ((r = fs_inode_read(ip, &ph, sizeof(ph), &off)) != sizeof(ph))
      goto out2;

    if (ph.type != PT_LOAD)
      continue;

    if (ph.filesz > ph.memsz) {
      r = -EINVAL;
      goto out2;
    }

    if ((ph.vaddr >= VIRT_KERNEL_BASE) || (ph.vaddr + ph.memsz > VIRT_KERNEL_BASE)) {
      r = -EINVAL;
      goto out2;
    }

    a = vm_range_alloc(vm->pgdir, ph.vaddr, ph.memsz, VM_READ | VM_WRITE | VM_EXEC | VM_USER);
    if (a < 0) {
      r = (int) a;
      goto out2;
    }

    heap = MAX(heap, ph.vaddr + ph.memsz);
    vm->heap = heap;

    if ((r = vm_space_load_inode(vm, (void *) ph.vaddr, ip, ph.filesz, ph.offset)) < 0)
      goto out2;
  }

  // Allocate user stack.
  if ((r = (int) vm_range_alloc(vm->pgdir, ustack, USTACK_SIZE, VM_READ | VM_WRITE | VM_USER)) < 0)
    return r;

  // Copy args and environment.
  usp = (char *) VIRT_USTACK_TOP;
  if ((r = copy_args(vm, argv, ustack, &usp)) < 0)
    goto out2;

  argc = r;
  uargv = usp;

  if ((r = copy_args(vm, envp, ustack, &usp)) < 0)
    goto out2;

  uenvp = usp;

  fs_inode_unlock_put(ip);

  proc = process_current();

  vm->heap  = heap;
  vm->stack = ustack;

  vm_load(vm->pgdir);
  vm_space_destroy(proc->vm);

  proc->vm = vm;

  fd_close_on_exec(proc);

  // Stack must be aligned to an 8-byte boundary in order for variadic args
  // to properly work!
  usp = ROUND_DOWN(usp, 8);

  arch_trap_frame_init(proc, elf.entry, argc, (uint32_t) uargv, (uint32_t) uenvp,
                     (uint32_t) usp);

  return argc;

out2:
  vm_space_destroy(vm);
out1:
  fs_inode_unlock_put(ip);

  return r;
}
