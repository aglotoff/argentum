#include <sys/fcntl.h>
#include <unistd.h>

int
_stat(const char *path, struct stat *buf)
{
  int fd, r;

  if ((fd = open(path, O_RDONLY)) < 0)
    return fd;

  r = fstat(fd, buf);

  close(fd);

  return r;
}

int
stat(const char *path, struct stat *buf)
{
  return _stat(path, buf);
}
