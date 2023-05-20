#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
fgetc(FILE *stream)
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

      stream->buf_size = BUFSIZ;
      stream->mode |= _MODE_ALLOC_BUF;
    }
  }

  // Can't call a read function if the last operation on the stream was a write
  if ((stream->write_end != NULL) || (stream->state & _STATE_EOF)) {
    stream->state |= _STATE_ERROR;
    return EOF;
  }

  // If there are pushback characters, return them first
  if (stream->back_count > 0) {
    if (--stream->back_count == 0) {
      stream->read_end  = stream->read_end;
      stream->read_save = NULL;
    }

    return stream->back[stream->back_count];
  }

  // Input buffer empty - try to read more bytes from the file descriptor
  if (stream->next >= stream->read_end) {
    ssize_t n = read(stream->fd, stream->buf, stream->buf_size);

    if (n < 0) {  // Input error
      stream->state |= _STATE_ERROR;
      return EOF;
    }

    if (n == 0) { // End of file
      stream->state |= _STATE_EOF;
      return EOF;
    }

    stream->next     = stream->buf;
    stream->read_end = stream->buf + n;
  }

  // Return the next character from the buffer
  return *stream->next++;
}
