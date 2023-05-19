#include <stdio.h>
#include <unistd.h>

int
__fflush(FILE *stream)
{
  ssize_t n;
  
  if (stream->next == stream->buf)
    return 0;

  n = write(stream->fd, stream->buf, stream->next - stream->buf);

  if (n < (stream->next - stream->buf)) {  // Input error
    stream->state |= _STATE_ERROR;

    stream->next      = NULL;
    stream->write_end = NULL;

    return EOF;
  }

  stream->next = stream->buf;

  return 0;
}
