#include <sys/syscall.h>
#include <unistd.h>

int
fchdir(int fildes)
{
  return __syscall1(__SYS_FCHDIR, fildes);
}
