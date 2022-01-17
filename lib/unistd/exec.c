#include <syscall.h>
#include <unistd.h>

int
exec(const char *path)
{
  return __syscall(__SYS_EXEC, (uint32_t) path, 0, 0);
}
