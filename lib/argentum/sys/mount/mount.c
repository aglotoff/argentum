#include <sys/mount.h>
#include <sys/syscall.h>

int
mount(const char *type, const char *path)
{
  return __syscall2(__SYS_MOUNT, type, path);
}
