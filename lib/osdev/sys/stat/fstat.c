#include <sys/stat.h>
#include <sys/syscall.h>

int
_fstat(int fildes, struct stat *buf)
{
  return __syscall(__SYS_STAT, fildes, (uintptr_t) buf, 0, 0, 0, 0);
}

int
fstat(int fildes, struct stat *buf)
{
  return _fstat(fildes, buf);
}
