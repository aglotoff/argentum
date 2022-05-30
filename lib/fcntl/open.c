#include <fcntl.h>
#include <stdarg.h>
#include <syscall.h>

int
open(const char *path, int oflag, ...)
{
  mode_t mode = 0;

  if (oflag & O_CREAT) {
    va_list ap;

    va_start(ap, oflag);
    mode = va_arg(ap, int);
    va_end(ap);

    // TODO: umask
  }

  return __syscall(__SYS_OPEN, (uint32_t) path, oflag, mode);
}
