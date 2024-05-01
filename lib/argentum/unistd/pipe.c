#include <unistd.h>
#include <sys/syscall.h>

int
pipe(int fildes[2])
{
  return __syscall1(__SYS_PIPE, fildes);
}
