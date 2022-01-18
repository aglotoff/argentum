#include <unistd.h>
#include <syscall.h>

ssize_t
read(int fildes, void *buf, size_t n)
{
  return __syscall(__SYS_READ, fildes, (uint32_t) buf, n);
}
