#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

struct File;

int32_t sys_dispatch(void);

int     sys_arg_fd(int n, int *, struct File **);
int     sys_arg_int(int, int *);
int     sys_arg_short(int, short *);
int     sys_arg_long(int, long *);
int     sys_arg_buf(int, void **, size_t, int);
int     sys_arg_str(int, const char **, int);

int32_t sys_read(void);
int32_t sys_write(void);
int32_t sys_exit(void);
int32_t sys_getpid(void);
int32_t sys_getppid(void);
int32_t sys_time(void);
int32_t sys_fork(void);
int32_t sys_wait(void);
int32_t sys_exec(void);
int32_t sys_open(void);
int32_t sys_chdir(void);
int32_t sys_getdents(void);
int32_t sys_stat(void);
int32_t sys_close(void);
int32_t sys_sbrk(void);
int32_t sys_mkdir(void);
int32_t sys_mknod(void);

#endif  // !__KERNEL_SYSCALL_H__
