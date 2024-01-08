#include <sys/syscall.h>
#include <unistd.h>

int
_unlink(const char *path)
{
  return __syscall(__SYS_UNLINK, (uint32_t) path, 0, 0);
}

int
unlink(const char *path)
{
  return _unlink(path);
}
