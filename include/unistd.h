#ifndef __INCLUDE_UNISTD_H__
#define __INCLUDE_UNISTD_H__

#include <sys/types.h>

pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
int   exec(const char *, char *const[]);

#endif  // !__INCLUDE_UNISTD_H__
