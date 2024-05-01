#include <sys/syscall.h>
#include <unistd.h>

off_t
_lseek(int fildes, off_t offset, int whence)
{
  return __syscall3(__SYS_SEEK, fildes, offset, whence);
}

off_t
lseek(int fildes, off_t offset, int whence)
{
  return _lseek(fildes, offset, whence);
}
