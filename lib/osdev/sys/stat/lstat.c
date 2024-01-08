#include <sys/fcntl.h>
#include <unistd.h>

int
lstat(const char *path, struct stat *buf)
{
  // TODO: do not use stat
  return stat(path, buf);
}
