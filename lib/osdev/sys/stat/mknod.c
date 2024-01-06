#include <sys/stat.h>
#include <sys/syscall.h>

int
mknod(const char *path, mode_t mode, dev_t dev)
{
  return __syscall(__SYS_MKNOD, (uintptr_t) path, mode, dev);
}
