#include <sys/syscall.h>
#include <unistd.h>

ssize_t
readlink(const char *path, char *buf, size_t bufsize)
{
  return __syscall3(__SYS_READLINK, path, buf, bufsize);
}
