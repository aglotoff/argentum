#include <sys/syscall.h>
#include <sys/types.h>

ssize_t
getdents(int fd, void *buf, size_t n)
{
  return __syscall3(__SYS_GETDENTS, fd, buf, n);
}
