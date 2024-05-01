#include <sys/syscall.h>
#include <unistd.h>

int
_execve(const char *path, char *const argv[], char *const envp[])
{
  return __syscall3(__SYS_EXEC, path, argv, envp);
}

int
execve(const char *path, char *const argv[], char *const envp[])
{
  return _execve(path, argv, envp);
}
