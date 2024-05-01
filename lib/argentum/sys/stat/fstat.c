#include <sys/stat.h>
#include <sys/syscall.h>

int
_fstat(int fildes, struct stat *buf)
{
  return __syscall2(__SYS_STAT, fildes, buf);
}

int
fstat(int fildes, struct stat *buf)
{
  return _fstat(fildes, buf);
}
