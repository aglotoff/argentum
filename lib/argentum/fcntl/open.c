#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>

int
_open(const char *path, int flags, ...)
{
  int mode = 0;

  if (flags & O_CREAT) {
    va_list ap;

    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);

    // TODO: umask
  }

  return __syscall(__SYS_OPEN, (uint32_t) path, flags, mode, 0, 0, 0);
}

int
open(const char *path, int flags, ...)
{
  int mode = 0;

  if (flags & O_CREAT) {
    va_list ap;

    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);

    // TODO: umask
  }

  return __syscall(__SYS_OPEN, (uint32_t) path, flags, mode, 0, 0, 0);
}
