#include <sys/stat.h>
#include <sys/syscall.h>

int
fchmod(int fildes, mode_t mode)
{
  return __syscall(__SYS_CHMOD, fildes, mode, 0, 0, 0, 0);
}
