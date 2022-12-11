#ifndef __INCLUDE_ARGENTUM_SYSCALL_H__
#define __INCLUDE_ARGENTUM_SYSCALL_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

int32_t sys_dispatch(void);

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
int32_t sys_umask(void);
int32_t sys_chdir(void);
int32_t sys_fchdir(void);
int32_t sys_getdents(void);
int32_t sys_link(void);
int32_t sys_unlink(void);
int32_t sys_rmdir(void);
int32_t sys_stat(void);
int32_t sys_close(void);
int32_t sys_sbrk(void);
int32_t sys_mknod(void);
int32_t sys_uname(void);
int32_t sys_chmod(void);

#endif  // !__INCLUDE_ARGENTUM_SYSCALL_H__
