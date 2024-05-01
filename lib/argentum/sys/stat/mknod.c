#include <sys/stat.h>
#include <sys/syscall.h>

int
mknod(const char *path, mode_t mode, dev_t dev)
{
  return __syscall3(__SYS_MKNOD, path, mode, dev);
}
