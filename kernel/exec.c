#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/elf.h>
#include <kernel/fs/fs.h>
#include <kernel/mm/memlayout.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>
#include <kernel/types.h>

static int
copy_args(tte_t *trtab, char *const args[], uintptr_t limit, char **sp)
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

    if ((r = vm_copy_out(trtab, p, args[i], n)) < 0)
      return r;

    oargs[i] = p;
  }
  oargs[i] = NULL;

  n = (i + 1) * sizeof(char *);
  p -= ROUND_UP(n, sizeof(uint32_t));
  
  if (p < (char *) (USTACK_TOP - USTACK_SIZE))
    return -E2BIG;

  if ((r = vm_copy_out(trtab, p, oargs, n)) < 0)
    return r;

  *sp = p;

  return i;
}

int
process_exec(const char *path, char *const argv[], char *const envp[])
{
  struct PageInfo *trtab_page;
  struct Process *proc;
  struct Inode *ip;
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  off_t off;
  tte_t *trtab;
  uintptr_t heap, ustack;
  char *usp, *uargv, *uenvp;
  int r, argc;

  if ((r = fs_name_lookup(path, &ip)) < 0)
    return r;

  fs_inode_lock(ip);

  if (!S_ISREG(ip->mode)) {
    r = -ENOENT;
    goto out1;
  }

  if ((trtab_page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL) {
    r = -ENOMEM;
    goto out1;
  }

  trtab = (tte_t *) page2kva(trtab_page);
  trtab_page->ref_count++;

  if ((r = fs_inode_read(ip, &elf, sizeof(elf), 0)) != sizeof(elf))
    goto out2;
  
  if (memcmp(elf.ident, "\x7f""ELF", 4) != 0) {
    r = -EINVAL;
    goto out2;
  }

  heap   = 0;
  ustack = USTACK_TOP - USTACK_SIZE;

  for (off = elf.phoff;
       (size_t) off < elf.phoff + elf.phnum * sizeof(ph);
       off += sizeof(ph)) {
    if ((r = fs_inode_read(ip, &ph, sizeof(ph), off)) != sizeof(ph))
      goto out2;

    if (ph.type != PT_LOAD)
      continue;

    if (ph.filesz > ph.memsz) {
      r = -EINVAL;
      goto out2;
    }

    if ((ph.vaddr >= KERNEL_BASE) || (ph.vaddr + ph.memsz > KERNEL_BASE)) {
      r = -EINVAL;
      goto out2;
    }

    if ((r = vm_alloc_region(trtab, (void *) ph.vaddr, ph.memsz,
                             VM_READ | VM_WRITE | VM_EXEC | VM_USER) < 0))
      goto out2;

    if ((r = vm_load(trtab, (void *) ph.vaddr, ip, ph.filesz, ph.offset)) < 0)
      goto out2;

    heap = MAX(heap, ph.vaddr + ph.memsz);
  }

  // Allocate user stack.
  if ((r = vm_alloc_region(trtab, (void *) ustack, USTACK_SIZE,
                           VM_READ | VM_WRITE | VM_USER)) < 0)
    return r;

  // Copy args and environment.
  usp = (char *) USTACK_TOP;
  if ((r = copy_args(trtab, argv, ustack, &usp)) < 0)
    goto out2;

  argc = r;
  uargv = usp;

  if ((r = copy_args(trtab, envp, ustack, &usp)) < 0)
    goto out2;

  uenvp = usp;

  fs_inode_unlock(ip);
  fs_inode_put(ip);

  proc = my_process();

  vm_switch_user(trtab);
  vm_free(proc->vm.trtab);

  proc->vm.trtab = trtab;
  proc->vm.heap  = heap;
  proc->vm.stack = ustack;

  // Stack must be aligned to an 8-byte boundary in order for variadic args
  // to properly work!
  usp = ROUND_DOWN(usp, 8);

  proc->tf->r0     = argc;                // arg #0: argc
  proc->tf->r1     = (uint32_t) uargv;    // arg #1: argv
  proc->tf->r2     = (uint32_t) uenvp;    // arg #2: environ
  proc->tf->sp_usr = (uint32_t) usp;      // stack pointer
  proc->tf->pc     = elf.entry;           // process entry point

  return argc;

out2:
  vm_free(trtab);

out1:
  fs_inode_unlock(ip);
  fs_inode_put(ip);

  return r;
}
