#ifndef __KERNEL_INCLUDE_KERNEL_SYSCALL_H__
#define __KERNEL_INCLUDE_KERNEL_SYSCALL_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

int     sys_arch_get_num(void);
int32_t sys_arch_get_arg(int);

int32_t sys_dispatch(void);

int32_t sys_exit(void);
int32_t sys_getpid(void);
int32_t sys_getppid(void);
int32_t sys_clock_time(void);
int32_t sys_fork(void);
int32_t sys_wait(void);
int32_t sys_exec(void);
int32_t sys_open(void);
int32_t sys_fcntl(void);
int32_t sys_umask(void);
int32_t sys_chdir(void);
int32_t sys_fchdir(void);
int32_t sys_link(void);
int32_t sys_unlink(void);
int32_t sys_rmdir(void);
int32_t sys_close(void);
int32_t sys_sbrk(void);
int32_t sys_mknod(void);
int32_t sys_uname(void);
int32_t sys_chmod(void);
int32_t sys_socket(void);
int32_t sys_bind(void);
int32_t sys_connect(void);
int32_t sys_listen(void);
int32_t sys_accept(void);
int32_t sys_test(void);
int32_t sys_sigaction(void);
int32_t sys_sigreturn(void);
int32_t sys_nanosleep(void);
int32_t sys_recvfrom(void);
int32_t sys_sendto(void);
int32_t sys_setsockopt(void);
int32_t sys_getuid(void);
int32_t sys_geteuid(void);
int32_t sys_getgid(void);
int32_t sys_getegid(void);
int32_t sys_getpgid(void);
int32_t sys_setpgid(void);
int32_t sys_access(void);
int32_t sys_pipe(void);
int32_t sys_mmap(void);
int32_t sys_select(void);
int32_t sys_sigpending(void);
int32_t sys_sigprocmask(void);
int32_t sys_sigsuspend(void);
int32_t sys_kill(void);
int32_t sys_readlink(void);
int32_t sys_times(void);
int32_t sys_mount(void);
int32_t sys_gethostbyname(void);
int32_t sys_setitimer(void);
int32_t sys_rename(void);
int32_t sys_chown(void);
int32_t sys_utime(void);
int32_t sys_symlink(void);
int32_t sys_ipc_send(void);
int32_t sys_ipc_sendv(void);

#endif  // !__KERNEL_INCLUDE_KERNEL_SYSCALL_H__
