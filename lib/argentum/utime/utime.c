#include <utime.h>
#include <sys/syscall.h>

int
utime(const char *path, const struct utimbuf *times)
{
  return __syscall2(__SYS_UTIME, path, times);
}
