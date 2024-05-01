#include <sys/syscall.h>
#include <unistd.h>

int
chdir(const char *path)
{
  return __syscall1(__SYS_CHDIR, path);
}
