#include <sys/syscall.h>
#include <unistd.h>

int
_close(int fildes)
{
  return __syscall1(__SYS_CLOSE, fildes);
}

int
close(int fildes)
{
  return _close(fildes);
}
