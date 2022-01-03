#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__

#include <stdint.h>

int32_t sys_dispatch(void);

int     sys_arg_int(int, int32_t *);
int     sys_arg_ptr(int, void **, size_t, int);
int     sys_arg_str(int, char **);

int32_t sys_cwrite(void);
int32_t sys_exit(void);
int32_t sys_getpid(void);
int32_t sys_getppid(void);
int32_t sys_time(void);
int32_t sys_fork(void);
int32_t sys_wait(void);

#endif  // !__KERNEL_SYSCALL_H__
