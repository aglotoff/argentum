#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

int
ioctl(int fildes, int request, ...)
{
  int arg = 0;

  if (IOCPARM_LEN(request) != 0) {
    va_list ap;

    va_start(ap, request);
    arg = va_arg(ap, int);
    va_end(ap);
  }

  return __syscall(__SYS_IOCTL, fildes, request, arg, 0, 0, 0);
}
