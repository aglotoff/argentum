#include <sys/syscall.h>
#include <unistd.h>

int
rmdir(const char *path)
{
  return __syscall(__SYS_RMDIR, (uint32_t) path, 0, 0, 0, 0, 0);
}
