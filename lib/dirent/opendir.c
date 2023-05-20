#include <dirent.h>
#include <fcntl.h>

DIR *
opendir(const char *dirname)
{
  int fd;
  DIR *dir;

  if ((fd = open(dirname, O_RDONLY)) < 0)
    return NULL;
  
  if ((dir = fdopendir(fd)) == NULL) {
    close(fd);
    return NULL;
  }

  return dir;
}
