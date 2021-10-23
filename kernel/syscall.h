#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__

int syscall(void);

int sys_arg_int(int, int *);
int sys_arg_ptr(int, void **, size_t);
int sys_arg_str(int, char **);

int sys_cputs(void);
int sys_exit(void);

#endif  // !__KERNEL_SYSCALL_H__
