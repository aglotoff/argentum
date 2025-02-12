#ifndef _ARCH_MEMLAYOUT_H
#define _ARCH_MEMLAYOUT_H

/** The number of bytes mapped by a single physical page. */
#define PAGE_SIZE         4096U
/** Log2 of PAGE_SIZE. */
#define PAGE_SHIFT        12

/** Size of a kernel-mode task stack in bytes */
#define KSTACK_SIZE       PAGE_SIZE

/** Size of a user-mode process stack in bytes */
// TODO: keep in sync with rlimit
#define USTACK_SIZE       (PAGE_SIZE * 32)

/** Physical address the kernel executable is loaded at */
#define PHYS_KERNEL_LOAD  0x00100000
/** Maximum physical memory available during the early boot process */
#define PHYS_ENTRY_LIMIT  0x00400000
/** Maximum available physical memory */
#define PHYS_LIMIT        0x10000000

/** All physical memory is mapped at this virtual address */
#define VIRT_KERNEL_BASE  0x80000000
/** Top of the user-mode process stack */
#define VIRT_USTACK_TOP   VIRT_KERNEL_BASE

#endif  // !_ARCH_MEMLAYOUT_H
