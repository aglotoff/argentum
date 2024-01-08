#include <sys/syscall.h>
#include <sys/types.h>

ssize_t
getdents(int fd, void *buf, size_t n)
{
  return __syscall(__SYS_GETDENTS, fd, (uint32_t) buf, n, 0, 0, 0);
}
