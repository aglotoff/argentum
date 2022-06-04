#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

FILE *
fopen(const char *pathname, const char *mode)
{
  int i, m;

  m = 0;

  if (*mode == 'r') {
    m |= _MODE_READ;
  } else if (*mode == 'w') {
    m |= _MODE_WRITE;
  } else if (*mode == 'a') {
    m |= (_MODE_WRITE | _MODE_APPEND);
  } else {
    errno = EINVAL;
    return NULL;
  }

  if (*++mode == 'b')
    mode++;
  if (*mode == '+')
    m |= (_MODE_READ | _MODE_WRITE);
  
  for (i = 0; i < FOPEN_MAX; i++) {
    FILE *fp;
    int oflag;

    if (__files[i] == NULL) {
      if ((fp = (FILE *) malloc(sizeof(FILE))) == NULL)
        return NULL;

      __files[i] = fp;
    } else if (__files[i]->_Mode == 0) {
      fp = __files[i];
    } else {
      continue;
    }

    oflag = 0;

    if ((m & (_MODE_READ | _MODE_WRITE)) == (_MODE_READ | _MODE_WRITE))
      oflag |= O_RDWR;
    else if (m & _MODE_READ)
      oflag |= O_RDONLY;
    else if (m & _MODE_WRITE)
      oflag |= O_WRONLY;

    if (m & _MODE_APPEND)
      oflag |= O_APPEND;
    if (m & _MODE_CREAT)
      oflag |= O_CREAT;
    if (m & _MODE_TRUNC)
      oflag |= O_TRUNC;

    if ((fp->_Fildes = open(pathname, oflag)) < 0) {
      // Cannot free static file structures for stdin, stdout,and stderr
      if (i > 2)
        free(fp);

      return NULL;
    }

    fp->_Mode = m;

    return fp;
  }

  errno = EMFILE;

  return NULL;
}
