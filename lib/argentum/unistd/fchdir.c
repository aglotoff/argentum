#include <sys/syscall.h>
#include <unistd.h>

int
fchdir(int fildes)
{
  return __syscall(__SYS_FCHDIR, fildes, 0, 0, 0, 0, 0);
}
