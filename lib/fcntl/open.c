#include <fcntl.h>
#include <syscall.h>

int
open(const char *path, int oflag)
{
  return __syscall(__SYS_OPEN, (uint32_t) path, oflag, 0);
}
