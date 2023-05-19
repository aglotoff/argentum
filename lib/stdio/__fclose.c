#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
__fclose(FILE *stream)
{
  if (stream->write_end != NULL)
    __fflush(stream);

  // Deallocate the buffer
  if (stream->mode & _MODE_ALLOC_BUF)
    free(stream->buf);

  return close(stream->fd);
}
