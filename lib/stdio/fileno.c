#include <errno.h>
#include <stdio.h>

int
fileno(FILE *stream)
{
  if (!stream->mode) {
    errno = EBADF;
    return -1;
  }

  return stream->fd;
}
