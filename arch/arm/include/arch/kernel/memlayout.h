#ifndef __AG_INCLUDE_ARCH_KERNEL_MEMLAYOUT_H__
#define __AG_INCLUDE_ARCH_KERNEL_MEMLAYOUT_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

/** Size of a kernel stack */
#define KSTACK_SIZE     4096

/** Size of a per-CPU exception stack */
#define XSTACK_SIZE     16
/** Offset of saved PSR in exception stack */
#define XSTACK_PSR      0
/** Offset of trap number in exception stack */
#define XSTACK_TRAPNO   4
/** Offset of saved R0 in exception stack */
#define XSTACK_R0       8
/** Offset of saved PC in exception stack */
#define XSTACK_PC       12

#endif  // !__AG_INCLUDE_ARCH_KERNEL_MEMLAYOUT_H__
