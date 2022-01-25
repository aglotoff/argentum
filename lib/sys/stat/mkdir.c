#include <syscall.h>
#include <sys/stat.h>

int
mkdir(const char *path, mode_t mode)
{
  return __syscall(__SYS_MKDIR, (uintptr_t) path, mode, 0);
}
