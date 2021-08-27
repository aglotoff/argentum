#ifndef KERNEL_MEMLAYOUT_H
#define KERNEL_MEMLAYOUT_H

/** All physical memory is mapped at this address. */
#define KERNEL_BASE   0x80000000

#define KSTACK_SIZE   4096  ///< Size of a per-process kernel stack

#endif  // !KERNEL_MEMLAYOUT_H
