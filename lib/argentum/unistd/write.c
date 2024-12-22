#include <sys/syscall.h>
#include <unistd.h>

_READ_WRITE_RETURN_TYPE
_write(int fildes, const void *buf, size_t n)
{
  return __syscall3(__SYS_WRITE, fildes, buf, n);
}

_READ_WRITE_RETURN_TYPE
write(int fildes, const void *buf, size_t n)
{
  return _write(fildes, buf, n);
}
