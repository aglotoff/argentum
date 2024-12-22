#ifndef __KERNEL_INCLUDE_KERNEL_MM_MEMLAYOUT_H__
#define __KERNEL_INCLUDE_KERNEL_MM_MEMLAYOUT_H__

/**
 * @file include/mm/memlayout.h
 * 
 * Common memory management definitions.
 */

#include <arch/memlayout.h>

#ifndef __ASSEMBLER__

#include <kernel/assert.h>
#include <stdint.h>

/** Integer type wide enough to represent a physical address. */
typedef unsigned long   physaddr_t;

static inline physaddr_t
__kva2pa(const char *file, int line, void *kva)
{
  if ((uintptr_t) kva < VIRT_KERNEL_BASE)
    __panic(file, line, "KVA2PA called with invalid kva %08lx", kva);
  return (physaddr_t) kva - VIRT_KERNEL_BASE;
}

/**
 * Given a kernel virtual address, get the corresponding physical address.
 *
 * @param va The kernel virtual address.
 *
 * @return The corresponding physical address.
 */
#define PA2KVA(va) __pa2kva(__FILE__, __LINE__, va)

static inline void *
__pa2kva(const char *file, int line, physaddr_t pa)
{
  if (pa >= VIRT_KERNEL_BASE)
    __panic(file, line, "PA2KVA called with invalid pa %08lx", pa);
  return (void *) (pa + VIRT_KERNEL_BASE);
}

/**
 * Given a physical address, get the corresponding kernel virtual address.
 *
 * @param va The physical address.
 *
 * @return The corresponding kernel virtual address.
 */
#define KVA2PA(pa) __kva2pa(__FILE__, __LINE__, pa)

#else

#define PA2KVA(pa)  ((pa) + VIRT_KERNEL_BASE)
#define KVA2PA(va)  ((va) - VIRT_KERNEL_BASE)

#endif  // !__ASSEMBLER__

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_MEMLAYOUT_H__
