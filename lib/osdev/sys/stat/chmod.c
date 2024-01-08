#include <sys/stat.h>
#include <sys/syscall.h>

int
chmod(const char *path, mode_t mode)
{
  return __syscall(__SYS_CHMOD, (uintptr_t) path, mode, 0, 0, 0, 0);
}
