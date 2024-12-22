#ifndef _ARCH_MEMLAYOUT_H
#define _ARCH_MEMLAYOUT_H

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE         4096U
/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT        12

/** Size of a kernel-mode thread stack in bytes */
#define KSTACK_SIZE       PAGE_SIZE

/** Size of a user-mode process stack in bytes */
// TODO: keep in sync with rlimit
#define USTACK_SIZE       (PAGE_SIZE * 16)

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

#define PHYS_EXTRA_BASE   0x20000000
#define PHYS_EXTRA_LIMIT  0x40000000

#define PHYS_CON0         0x10002000    ///< 3-Wire Serial Bus Control
#define PHYS_MMCI         0x10005000    ///< MultiMedia Card Interface
#define PHYS_KMI0         0x10006000    ///< Keyboard/Mouse Interface 0
#define PHYS_UART0        0x10009000    ///< UART 0 Interface
#define PHYS_LCD          0x10020000    ///< Color LCD Controller configuration
#define PHYS_ETH          0x4E000000    ///< Static memory (CS3) Ethernet

/** Exception vectors are mapped at this virtual address */
#define VIRT_VECTOR_BASE  0xFFFF0000
/** All physical memory is mapped at this virtual address */
#define VIRT_KERNEL_BASE  0x80000000
/** Top of the user-mode process stack */
#define VIRT_USTACK_TOP   VIRT_KERNEL_BASE

#endif  // !_ARCH_MEMLAYOUT_H
