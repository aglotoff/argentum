#ifndef __AG_INCLUDE_ARCH_KERNEL_VM_H__
#define __AG_INCLUDE_ARCH_KERNEL_VM_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE         4096U
/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT        12

/** Physical address the kernel executable is loaded at */
#define PHYS_KERNEL_LOAD  0x00010000
/** Maximum physical memory available during the early boot process */
#define PHYS_ENTRY_LIMIT  0x01000000
/** Maximum available physical memory */
#define PHYS_LIMIT        0x10000000

/** Exception vectors are mapped at this virtual address */
#define VIRT_VECTOR_BASE  0xFFFF0000
/** All physical memory is mapped at this virtual address */
#define VIRT_KERNEL_BASE  0x80000000
/** Top of the user-mode process stack */
#define VIRT_USTACK_TOP   VIRT_KERNEL_BASE

#ifndef __ASSEMBLER__

#include <assert.h>
#include <stdint.h>

/** Integer type wide enough to represent a physical address. */
typedef unsigned long   physaddr_t;

static inline physaddr_t
arch_kva2pa(const char *file, int line, void *kva)
{
  if ((uintptr_t) kva < VIRT_KERNEL_BASE)
    _Panic(file, line, "KVA2PA called with invalid kva %08lx", kva);
  return (physaddr_t) kva - VIRT_KERNEL_BASE;
}

static inline void *
arch_pa2kva(const char *file, int line, physaddr_t pa)
{
  if (pa >= VIRT_KERNEL_BASE)
    _Panic(file, line, "PA2KVA called with invalid pa %08lx", pa);
  return (void *) (pa + VIRT_KERNEL_BASE);
}

void         arch_vm_init(void);
void         arch_vm_init_percpu(void);
int          arch_vm_pte_valid(void *);
physaddr_t   arch_vm_pte_addr(void *);
int          arch_vm_pte_flags(void *);
void         arch_vm_pte_set(void *, physaddr_t, int);
void         arch_vm_pte_clear(void *);
void        *arch_vm_lookup(void *, uintptr_t, int);
void        *arch_vm_create(void);
void         arch_vm_destroy(void *);
void         arch_vm_load(void *);
void         arch_vm_invalidate(uintptr_t);

#endif  // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_ARCH_KERNEL_VM_H__
