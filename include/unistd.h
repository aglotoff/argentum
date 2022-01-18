#ifndef __INCLUDE_UNISTD_H__
#define __INCLUDE_UNISTD_H__

#include <sys/types.h>

pid_t   fork(void);
pid_t   getpid(void);
pid_t   getppid(void);
int     exec(const char *, char *const[]);

ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);

#endif  // !__INCLUDE_UNISTD_H__
