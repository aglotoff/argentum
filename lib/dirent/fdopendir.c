#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

DIR *
fdopendir(int fd)
{
  struct stat stat;
  DIR *dir;

  if ((fstat(fd, &stat)) < 0)
    return NULL;
  
  if (!S_ISDIR(stat.st_mode))
    return NULL;
  
  if ((dir = (DIR *) malloc(sizeof *dir)) == NULL)
    return NULL;

  dir->_Fd      = fd;
  dir->_Buf_end = NULL;
  dir->_Next    = NULL;

  return dir;
}
