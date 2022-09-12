#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int
__fclose(FILE *stream)
{
  // TODO: flush stream

  // TODO: deallocate buffers

  return close(stream->fd);
}
