#include <unistd.h>
#include <sys/syscall.h>

int
access(const char *path, int amode)
{
  return __syscall(__SYS_ACCESS, (uintptr_t) path, amode, 0, 0, 0, 0);
}
