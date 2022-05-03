#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int
fclose(FILE *stream)
{
  if (stream->_Mode == 0) {
    errno = EBADF;
    return -1;
  }

  // TODO: flush stream

  // TODO: deallocate buffers

  stream->_Mode = 0;

  return close(stream->_Fildes);
}
