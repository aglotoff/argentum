#include <sys/syscall.h>
#include <unistd.h>

off_t
_lseek(int fildes, off_t offset, int whence)
{
  return __syscall(__SYS_SEEK, fildes, offset, whence, 0, 0, 0);
}

off_t
lseek(int fildes, off_t offset, int whence)
{
  return _lseek(fildes, offset, whence);
}
