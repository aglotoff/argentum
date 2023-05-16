#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

extern char **environ;

pid_t    fork(void);
pid_t    getpid(void);
pid_t    getppid(void);

int      execl(const char *, ...);
int      execle(const char *, ...);
int      execlp(const char *, ...);
int      execv(const char *, char *const[]);
int      execve(const char *, char *const[], char *const[]);
int      execvp(const char *, char *const[]);

unsigned alarm(unsigned);

int      chdir(const char *);
int      fchdir(int);
char    *getcwd(char *, size_t);
int      link(const char *,const char *);
int      unlink(const char *);
int      rmdir(const char *);

int      close(int);
ssize_t  read(int, void *, size_t);
ssize_t  write(int, const void *, size_t);

void    *sbrk(ptrdiff_t);
int      pipe(int[2]);
int      isatty(int);
void     _exit(int);
int      dup(int);
int      access(const char *, int);
off_t    lseek(int, off_t, int);

#endif  // !__UNISTD_H__
