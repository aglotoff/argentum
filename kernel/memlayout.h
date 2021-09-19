#ifndef KERNEL_MEMLAYOUT_H
#define KERNEL_MEMLAYOUT_H

/** Assume that we have at least 256M of physical memory */
#define PHYS_TOP        (256 * 1024 * 1024)

/** During the boot time, we can access only up to 16MB of physical memory */
#define PHYS_ENTRY_TOP  (16 * 1024 * 1024)

/** All physical memory is mapped at this address. */
#define KERNEL_BASE     0x80000000

#define VECTORS_BASE    0xFFFF0000

#define KSTACK_SIZE     4096  ///< Size of a per-process kernel stack

#define KXSTACK_SIZE    16    ///< Size of a per-process exception stack
#define KXSTACK_R0      0
#define KXSTACK_SPSR    4
#define KXSTACK_LR      8
#define KXSTACK_TRAPNO  12

#define USTACK_TOP      KERNEL_BASE
#define USTACK_SIZE     (PAGE_SIZE * 4)

#define MMIO_LIMIT      VECTORS_BASE
#define MMIO_BASE       (MMIO_LIMIT - 16 * 1024 * 1024)

#ifndef __ASSEMBLER__

#include <assert.h>
#include <stdint.h>

static inline uint32_t
__paddr(const char *file, int line, void *va)
{
  if ((uintptr_t) va < KERNEL_BASE)
    __panic(file, line, "PADDR called with invalid kva %08lx", va);
  return (uint32_t) va - KERNEL_BASE;
}

#define PADDR(va) __paddr(__FILE__, __LINE__, va)

static inline void *
__kaddr(const char *file, int line, uint32_t pa)
{
  if (pa >= KERNEL_BASE)
    __panic(file, line, "KADDR called with invalid pa %08lx", pa);
  return (void *) (pa + KERNEL_BASE);
}

#define KADDR(pa) __kaddr(__FILE__, __LINE__, pa)

#endif  // !__ASSEMBLER__

#endif  // !KERNEL_MEMLAYOUT_H
