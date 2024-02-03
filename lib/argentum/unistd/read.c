#include <sys/syscall.h>
#include <unistd.h>

ssize_t
_read(int fildes, void *buf, size_t n)
{
  return __syscall(__SYS_READ, fildes, (uint32_t) buf, n, 0, 0, 0);
}

ssize_t
read(int fildes, void *buf, size_t n)
{
  return _read(fildes, buf, n);
}
