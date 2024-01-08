#include <sys/syscall.h>
#include <unistd.h>

int
chdir(const char *path)
{
  return __syscall(__SYS_CHDIR, (uint32_t) path, 0, 0, 0, 0, 0);
}
