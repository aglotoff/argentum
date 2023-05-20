#include <unistd.h>
#include <syscall.h>

off_t
lseek(int fildes, off_t offset, int whence)
{
  return __syscall(__SYS_SEEK, fildes, offset, whence);
}
