#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__

int sys_dispatch(void);

int sys_arg_int(int, long *);
int sys_arg_ptr(int, void **, size_t);
int sys_arg_str(int, char **);

int sys_cwrite(void);
int sys_exit(void);
int sys_getpid(void);
int sys_getppid(void);
int sys_time(void);
int sys_fork(void);

#endif  // !__KERNEL_SYSCALL_H__
