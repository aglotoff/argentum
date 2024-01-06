#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>

int
fcntl(int fildes, int cmd, ...)
{
  int arg;
  va_list ap;

  switch (cmd) {
  case F_SETFD:
    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);
    break;
  default:
    arg = 0;
    break;
  }

  return __syscall(__SYS_FCNTL, fildes, cmd, arg);
}
