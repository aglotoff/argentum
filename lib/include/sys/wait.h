#ifndef __SYS_WAIT__
#define __SYS_WAIT__

/**
 * @file include/sys/wait.h
 * 
 * Declarations for waiting
 */

#define WNOHANG       (1 << 0)
#define WUNTRACED     (1 << 1)

#define __WEXITED     0
#define __WSIGNALED   1
#define __WSTOPPED    2

#define __WCODE(stat_val)       ((stat_val) & 0xff)
#define __WSTATUS(stat_val)     (((stat_val) >> 8) & 0xff)

#define WIFEXITED(stat_val)     (__WSTATUS(stat_val) == __WEXITED)
#define WEXITSTATUS(stat_val)   __WCODE(stat_val)
#define WIFSIGNALED(stat_val)   (__WSTATUS(stat_val) == __WSIGNALED)
#define WTERMSIG(stat_val)      __WCODE(stat_val)
#define WIFSTOPPED(stat_val)    (__WSTATUS(stat_val) == __WSTOPPED)
#define WTERMSIG(stat_val)      __WCODE(stat_val)

/** Used for process IDs and process group IDs. */
typedef int   pid_t;

pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);

#endif  // !__SYS_WAIT__
