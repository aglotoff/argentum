#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int
stat(const char *path, struct stat *buf)
{
  int fd, r;

  if ((fd = open(path, O_RDONLY)) < 0)
    return fd;
  
  r = fstat(fd, buf);

  close(fd);

  return r;
}
