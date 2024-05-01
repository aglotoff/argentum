#include <unistd.h>
#include <sys/syscall.h>

int
access(const char *path, int amode)
{
  return __syscall2(__SYS_ACCESS, path, amode);
}
