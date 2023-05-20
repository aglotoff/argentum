#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int
closedir(DIR *dirp)
{
  if ((dirp == NULL) || (dirp->_Fd < 0)) {
    errno = EBADF;
    return -1;
  }
  
  if (close(dirp->_Fd) < 0)
    return -1;

  free(dirp);

  return 0;
}
