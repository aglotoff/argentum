#include <sys/syscall.h>
#include <unistd.h>

ssize_t
_write(int fildes, const void *buf, size_t n)
{
  return __syscall3(__SYS_WRITE, fildes, buf, n);
}

ssize_t
write(int fildes, const void *buf, size_t n)
{
  return _write(fildes, buf, n);
}
