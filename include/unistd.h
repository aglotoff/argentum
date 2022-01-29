#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stddef.h>
#include <sys/types.h>

extern char **environ;

pid_t   fork(void);
pid_t   getpid(void);
pid_t   getppid(void);

int     execl(const char *, ...);
int     execle(const char *, ...);
int     execlp(const char *, ...);
int     execv(const char *, char *const[]);
int     execve(const char *, char *const[], char *const[]);
int     execvp(const char *, char *const[]);

int     chdir(const char *);

int     close(int);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);

void   *sbrk(ptrdiff_t);

#endif  // !__UNISTD_H__
