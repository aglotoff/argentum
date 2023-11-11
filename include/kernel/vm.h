#ifndef __AG_INCLUDE_KERNEL_VM_H__
#define __AG_INCLUDE_KERNEL_VM_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <arch/kernel/vm.h>

// Master kernel page table.
extern void *kernel_pgtab;

struct Page;

/**
 * Given a kernel virtual address, get the corresponding physical address.
 *
 * @param va The kernel virtual address.
 *
 * @return The corresponding physical address.
 */
#define PA2KVA(va) arch_pa2kva(__FILE__, __LINE__, va)

/**
 * Given a physical address, get the corresponding kernel virtual address.
 *
 * @param va The physical address.
 *
 * @return The corresponding kernel virtual address.
 */
#define KVA2PA(pa) arch_kva2pa(__FILE__, __LINE__, pa)

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

struct Page *vm_page_lookup(void *, uintptr_t, int *);
int          vm_page_insert(void *, struct Page *, uintptr_t, int);
int          vm_page_remove(void *, uintptr_t);

#endif  // !__AG_INCLUDE_KERNEL_VM_H__
