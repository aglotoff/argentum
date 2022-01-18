#include <unistd.h>
#include <syscall.h>

ssize_t
write(int fildes, const void *buf, size_t n)
{
  return __syscall(__SYS_WRITE, fildes, (uint32_t) buf, n);
}
