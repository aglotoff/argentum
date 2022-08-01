#ifndef __KERNEL_MM_MEMLAYOUT_H__
#define __KERNEL_MM_MEMLAYOUT_H__

/**
 * @file kernel/include/mm/memlayout.h
 * 
 * Common memory management definitions.
 */

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE         4096U
/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT        12

/** Size of a kernel-mode thread stack in bytes */
#define KSTACK_SIZE       PAGE_SIZE

/** Size of a user-mode process stack in bytes */
#define USTACK_SIZE       (PAGE_SIZE * 4)

/** Size of a per-CPU exception stack in bytes */
#define KXSTACK_SIZE      16

#define KXSTACK_PSR       0       ///< Offset of saved PSR in exception stack
#define KXSTACK_TRAPNO    4       ///< Offset of trap number in exception stack
#define KXSTACK_R0        8       ///< Offset of saved R0 in exception stack
#define KXSTACK_PC        12      ///< Offset of saved PC in exception stack

/** Physical address the kernel executable is loaded at */
#define PHYS_KERNEL_LOAD  0x00010000
/** Maximum physical memory available during the early boot process */
#define PHYS_ENTRY_LIMIT  0x01000000
/** Maximum available physical memory */
#define PHYS_LIMIT        0x10000000

#define PHYS_CON0         0x10002000    ///< 3-Wire Serial Bus Control
#define PHYS_MMCI         0x10005000    ///< MultiMedia Card Interface
#define PHYS_KMI0         0x10006000    ///< Keyboard/Mouse Interface 0
#define PHYS_UART0        0x10009000    ///< UART 0 Interface
#define PHYS_LCD          0x10020000    ///< Color LCD Controller configuration
#define PHYS_GICC         0x1F000100    ///< Interrupt interface
#define PHYS_PTIMER       0x1F000600    ///< Private timer
#define PHYS_GICD         0x1F001000    ///< Distributor
#define PHYS_ETH          0x4E000000    ///< Static memory (CS3) Ethernet

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
__pa2kva(const char *file, int line, void *kva)
{
  if ((uintptr_t) kva < VIRT_KERNEL_BASE)
    __panic(file, line, "PA2KVA called with invalid kva %08lx", kva);
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
__kva2pa(const char *file, int line, physaddr_t pa)
{
  if (pa >= VIRT_KERNEL_BASE)
    __panic(file, line, "KVA2PA called with invalid pa %08lx", pa);
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

#endif  // !__KERNEL_MM_MEMLAYOUT_H__
