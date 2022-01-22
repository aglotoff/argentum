#include <syscall.h>
#include <unistd.h>

int
execv(const char *path, char *const argv[])
{
  return __syscall(__SYS_EXEC, (uint32_t) path, (uint32_t) argv, 0);
}
