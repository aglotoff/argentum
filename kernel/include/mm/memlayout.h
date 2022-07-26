#ifndef __KERNEL_MM_MEMLAYOUT_H__
#define __KERNEL_MM_MEMLAYOUT_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE       4096U

/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT      12

/** Assume that we have at least 256M of physical memory */
#define PHYS_TOP        0x10000000

/** During the boot time, we can access only up to 16MB of physical memory */
#define PHYS_ENTRY_TOP  (16 * 1024 * 1024)

/** Kernel executable is loaded in memory at this physical address. */
#define KERNEL_LOAD     0x10000

/** All physical memory is mapped at this address. */
#define KERNEL_BASE     0x80000000

#define VECTORS_BASE    0xFFFF0000

#define KSTACK_SIZE     4096  ///< Size of a per-process kernel stack

#define KXSTACK_SIZE    16    ///< Size of a per-process exception stack
#define KXSTACK_PSR     0
#define KXSTACK_TRAPNO  4
#define KXSTACK_R0      8
#define KXSTACK_PC      12

#define USTACK_TOP      KERNEL_BASE
#define USTACK_SIZE     (PAGE_SIZE * 4)

#define SB_CON0       0x10002000    ///< SBCon0 memory base address
#define MMCI_BASE     0x10005000    ///< MCI base address
#define KMI0_BASE     0x10006000  
#define UART0         0x10009000   
#define LCD_BASE      0x10020000    ///< LCD base memory address
#define GICC_BASE     0x1F000100    ///< Interrupt interface
#define PTIMER_BASE   0x1F000600    ///< Private timer
#define GICD_BASE     0x1F001000    ///< Distributor
#define ETH_BASE      0x4E000000

#ifndef __ASSEMBLER__

#include <assert.h>
#include <stdint.h>

/** Integer type wide enough to represent a physical address. */
typedef unsigned long   physaddr_t;

static inline physaddr_t
__paddr(const char *file, int line, void *va)
{
  if ((uintptr_t) va < KERNEL_BASE)
    __panic(file, line, "PADDR called with invalid kva %08lx", va);
  return (physaddr_t) va - KERNEL_BASE;
}

#define PADDR(va) __paddr(__FILE__, __LINE__, va)

static inline void *
__kaddr(const char *file, int line, physaddr_t pa)
{
  if (pa >= KERNEL_BASE)
    __panic(file, line, "KADDR called with invalid pa %08lx", pa);
  return (void *) (pa + KERNEL_BASE);
}

#define KADDR(pa) __kaddr(__FILE__, __LINE__, pa)

#endif  // !__ASSEMBLER__

#endif  // !__KERNEL_MM_MEMLAYOUT_H__
