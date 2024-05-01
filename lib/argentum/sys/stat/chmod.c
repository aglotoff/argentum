#include <sys/stat.h>
#include <sys/syscall.h>

int
chmod(const char *path, mode_t mode)
{
  return __syscall2(__SYS_CHMOD, path, mode);
}
