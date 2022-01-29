#include <syscall.h>
#include <unistd.h>

int
execve(const char *path, char *const argv[], char *const envp[])
{
  return __syscall(__SYS_EXEC, (uint32_t) path, (uint32_t) argv,
                   (uint32_t) envp);
}
