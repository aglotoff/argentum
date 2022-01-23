#include <syscall.h>
#include <sys/stat.h>

int
fstat(int fildes, struct stat *buf)
{
  return __syscall(__SYS_STAT, fildes, (uintptr_t) buf, 0);
}
