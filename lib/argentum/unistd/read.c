#include <sys/syscall.h>
#include <unistd.h>

_READ_WRITE_RETURN_TYPE
_read(int fildes, void *buf, size_t n)
{
  return __syscall3(__SYS_READ, fildes, buf, n);
}

_READ_WRITE_RETURN_TYPE
read(int fildes, void *buf, size_t n)
{
  return _read(fildes, buf, n);
}
