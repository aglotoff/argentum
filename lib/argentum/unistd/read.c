#include <sys/syscall.h>
#include <unistd.h>

ssize_t
_read(int fildes, void *buf, size_t n)
{
  return __syscall3(__SYS_READ, fildes, buf, n);
}

ssize_t
read(int fildes, void *buf, size_t n)
{
  return _read(fildes, buf, n);
}
