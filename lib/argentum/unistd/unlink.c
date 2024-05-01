#include <sys/syscall.h>
#include <unistd.h>

int
_unlink(const char *path)
{
  return __syscall1(__SYS_UNLINK, path);
}

int
unlink(const char *path)
{
  return _unlink(path);
}
