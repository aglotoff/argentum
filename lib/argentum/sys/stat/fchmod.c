#include <sys/stat.h>
#include <sys/syscall.h>

int
fchmod(int fildes, mode_t mode)
{
  return __syscall2(__SYS_CHMOD, fildes, mode);
}
