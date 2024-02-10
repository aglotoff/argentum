#ifndef __KERNEL_INCLUDE_KERNEL_MM_VM_H__
#define __KERNEL_INCLUDE_KERNEL_MM_VM_H__

#ifndef __ARGENTUM_KERNEL__
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
int          vm_range_alloc(void *, uintptr_t, size_t, int);
void         vm_range_free(void *, uintptr_t, size_t);
int          vm_range_clone(void *, void *, uintptr_t, size_t, int);
int          vm_copy_out(void *, void *, const void *, size_t);
int          vm_copy_in(void *, void *, const void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_VM_H__
