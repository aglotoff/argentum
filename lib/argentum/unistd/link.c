#include <sys/syscall.h>
#include <unistd.h>

int
_link(const char *path1, const char *path2)
{
  return __syscall2(__SYS_LINK, path1, path2);
}

int
link(const char *path1, const char *path2)
{
  return _link(path1, path2);
}
