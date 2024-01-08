#include <sys/syscall.h>
#include <unistd.h>

int
_execve(const char *path, char *const argv[], char *const envp[])
{
  return __syscall(__SYS_EXEC, (uint32_t) path, (uint32_t) argv,
                   (uint32_t) envp, 0, 0, 0);
}

int
execve(const char *path, char *const argv[], char *const envp[])
{
  return _execve(path, argv, envp);
}
