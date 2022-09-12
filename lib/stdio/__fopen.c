#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int
__fopen(FILE *fp, const char *pathname, const char *mode)
{
  int fd, oflag, mode_bits;

  mode_bits = 0;

  if (*mode == 'r') {
    mode_bits |= _MODE_READ;
  } else if (*mode == 'w') {
    mode_bits |= _MODE_WRITE;
  } else if (*mode == 'a') {
    mode_bits |= (_MODE_WRITE | _MODE_APPEND);
  } else {
    errno = EINVAL;
    return -1;
  }

  if (*++mode == 'b')
    mode++;
  if (*mode == '+')
    mode_bits |= (_MODE_READ | _MODE_WRITE);
  
  oflag = 0;

  if ((mode_bits & (_MODE_READ | _MODE_WRITE)) == (_MODE_READ | _MODE_WRITE))
    oflag |= O_RDWR;
  else if (mode_bits & _MODE_READ)
    oflag |= O_RDONLY;
  else if (mode_bits & _MODE_WRITE)
    oflag |= O_WRONLY;

  if (mode_bits & _MODE_APPEND)
    oflag |= O_APPEND;
  if (mode_bits & _MODE_CREAT)
    oflag |= O_CREAT;
  if (mode_bits & _MODE_TRUNC)
    oflag |= O_TRUNC;

  if ((fd = open(pathname, oflag)) < 0)
    return fd;

  fp->mode |= mode_bits;
  fp->fd    = fd;

  // TODO: init other fields
    
  return 0;
}
