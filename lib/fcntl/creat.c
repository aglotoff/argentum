#include <fcntl.h>
#include <sys/stat.h>

int
creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
