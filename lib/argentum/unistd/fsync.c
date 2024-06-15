#include <sys/syscall.h>
#include <unistd.h>

int 
_fsync(int fildes)
{
  return __syscall1(__SYS_FSYNC, fildes);
}

int
fsync(int fildes)
{
  return _fsync(fildes);
}
