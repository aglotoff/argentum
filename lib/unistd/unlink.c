#include <syscall.h>
#include <unistd.h>

int
unlink(const char *path)
{
  return __syscall(__SYS_UNLINK, (uint32_t) path, 0, 0);
}
