#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
fputc(int c, FILE *stream)
{
  // Try to allocate buffer. If the stream should not be buffered, or if we are
  // out of memory, fall back to the single-character buffer
  if (stream->buf == NULL) {
    if (stream->mode & _MODE_NO_BUF) {
      stream->buf = stream->char_buf;
      stream->buf_size = 1;
    } else {
      if (stream->buf_size == 0)
        stream->buf_size = BUFSIZ;
      
      if ((stream->buf = (char *) malloc(stream->buf_size)) == NULL)
        return EOF;

      stream->mode |= _MODE_ALLOC_BUF;
    }
  }

  if (stream->write_end == NULL) {
    if (stream->read_end != NULL) {
      // Can't call a write function if the last operation on the stream was a
      // read and there were no EOF
      if (!(stream->state & _STATE_EOF)) {
        stream->state |= _STATE_ERROR;
        return EOF;
      }

      stream->state &= ~_STATE_EOF;
      stream->read_end = NULL;
      stream->back_count = 0;
    }

    stream->next      = stream->buf;
    stream->write_end = stream->buf + stream->buf_size;
  }

  if ((stream->next >= stream->write_end) && (__fflush(stream) == EOF))
    return EOF;

  *stream->next++ = c;

  if ((stream->next >= stream->write_end) && (__fflush(stream) == EOF))
    return EOF;

  // Return the character just written
  return c;
}
