#include <fcntl.h>
#include <syscall.h>
#include <sys/stat.h>

int
creat(const char *path, mode_t mode)
{
  return __syscall(__SYS_OPEN, (uint32_t) path, O_WRONLY | O_CREAT | O_TRUNC,
                   mode);
}
