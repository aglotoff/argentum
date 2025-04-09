#include <unistd.h>
#include <sys/syscall.h>

int
symlink(const char *path1, const char *path2)
{
  return __syscall2(__SYS_SYMLINK, path1, path2);
}
