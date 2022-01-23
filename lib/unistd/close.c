#include <fcntl.h>
#include <syscall.h>

int
close(int fildes)
{
  return __syscall(__SYS_CLOSE, fildes, 0, 0);
}
