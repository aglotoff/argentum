#include <sys/syscall.h>
#include <unistd.h>

int
_link(const char *path1, const char *path2)
{
  return __syscall(__SYS_LINK, (uint32_t) path1, (uint32_t) path2, 0, 0, 0, 0);
}

int
link(const char *path1, const char *path2)
{
  return _link(path1, path2);
}
