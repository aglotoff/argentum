#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>

int
fcntl(int fildes, int cmd, ...)
{
  long arg;
  va_list ap;

  va_start(ap, cmd);

  // Fetch the third argument for operations that require it
  switch (cmd) {
  case F_DUPFD:
  case F_SETFD:
  case F_SETFL:
  case F_SETOWN:
    arg = va_arg(ap, int);
    break;
  case F_GETLK:
  case F_SETLK:
  case F_SETLKW:
    arg = va_arg(ap, uintptr_t);
    break;
  default:
    arg = 0;
    break;
  }

  va_end(ap);

  return __syscall3(__SYS_FCNTL, fildes, cmd, arg);
}
