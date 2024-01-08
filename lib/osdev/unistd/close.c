#include <sys/syscall.h>
#include <unistd.h>

int
_close(int fildes)
{
  return __syscall(__SYS_CLOSE, fildes, 0, 0, 0, 0, 0);
}

int
close(int fildes)
{
  return _close(fildes);
}
