#ifndef KERNEL_MEMLAYOUT_H
#define KERNEL_MEMLAYOUT_H

/** All physical memory is mapped at this address. */
#define KERNEL_BASE   0x80000000

#define KSTACK_SIZE   4096  ///< Size of a per-process kernel stack

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
