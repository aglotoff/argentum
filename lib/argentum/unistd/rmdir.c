#include <sys/syscall.h>
#include <unistd.h>

int
rmdir(const char *path)
{
  return __syscall1(__SYS_RMDIR, path);
}
