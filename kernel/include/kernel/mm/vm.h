#ifndef __KERNEL_INCLUDE_KERNEL_MM_VM_H__
#define __KERNEL_INCLUDE_KERNEL_MM_VM_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stddef.h>
#include <sys/types.h>

#include <kernel/elf.h>
#include <kernel/list.h>
#include <kernel/armv7/mmu.h>

struct Page;

/** Virtual memory mapping flags */
enum {
  /** Readable */
  VM_READ      = (1 << 0),
  /** Writeable */
  VM_WRITE     = (1 << 1),
  /** Accessible from user mode */
  VM_USER      = (1 << 2),
  /** Executable */
  VM_EXEC      = (1 << 3),
  /** Disable cacheing */
  VM_NOCACHE   = (1 << 4),
  /** Copy-on-write */
  VM_COW       = (1 << 5),
  /** Page mapping (i.e. not a file or MMIO address) */
  __VM_PAGE    = (1 << 6),
};

void         vm_init(void);
struct Page *vm_page_lookup(void *, uintptr_t, int *);
int          vm_page_insert(void *, struct Page *, uintptr_t, int);
int          vm_page_remove(void *, uintptr_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_VM_H__
