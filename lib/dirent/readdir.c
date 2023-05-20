#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

struct dirent *
readdir(DIR *dirp)
{
  struct dirent *de;
  
  if (dirp->_Next >= dirp->_Buf_end) {
    ssize_t nread;
    
    if ((nread = getdents(dirp->_Fd, dirp->_Buf, _DIRENT_MAX)) <= 0) {
      dirp->_Next    = NULL;
      dirp->_Buf_end = NULL;
      return NULL;
    }

    dirp->_Next    = dirp->_Buf;
    dirp->_Buf_end = dirp->_Buf + nread;
  }

  de = (struct dirent *) dirp->_Next;
  dirp->_Next += de->d_reclen;

  return de;
}
