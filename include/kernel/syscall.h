#ifndef __AG_INCLUDE_KERNEL_SYSCALL_H__
#define __AG_INCLUDE_KERNEL_SYSCALL_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <arch/kernel/syscall.h>

long syscall_dispatch(void *);

#endif  // !__AG_INCLUDE_KERNEL_SYSCALL_H__
