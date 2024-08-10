#include <sys/stat.h>
#include <sys/syscall.h>

int
mkfifo(const char *path, mode_t mode)
{
  return __syscall2(__SYS_MKNOD, path, S_IFIFO | mode);
}
