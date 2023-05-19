#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int
setvbuf(FILE *stream, char *buf, int type, size_t size)
{
  if ((stream->mode == 0) || (stream->buf != NULL)) {
    errno = EBADF;
    return -1;
  }

  if (type == _IONBF) {
    stream->mode    |= _MODE_NO_BUF;
    stream->buf      = stream->char_buf;
    stream->buf_size = 1;
    return 0;
  } 

  stream->mode |= (type == _IOLBF) ? _MODE_LINE_BUF : _MODE_FULL_BUF;

  if (size != 0) {
    stream->buf      = buf;
    stream->buf_size = size;
  }

  return 0;
}
