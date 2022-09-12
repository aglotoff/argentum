#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int
fclose(FILE *stream)
{
  if (stream->mode == 0) {
    errno = EBADF;
    return -1;
  }

  if (__fclose(stream) != 0)
    return -1;

  stream->mode = 0;

  __ffree(stream);

  return 0;
}
