#ifndef __AG_INCLUDE_ARCH_KERNEL_SYSCALL_H__
#define __AG_INCLUDE_ARCH_KERNEL_SYSCALL_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

int  arch_syscall_no(void *);
long arch_syscall_arg(void *, int);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_SYSCALL_H__
