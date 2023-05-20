#include <fcntl.h>
#include <stdio.h>

int
fseeko(FILE *stream, off_t offset, int whence)
{
  if ((stream->write_end != NULL) && (__fflush(stream) < 0))
    return -1;
  
  if (lseek(stream->fd, offset, whence) < 0)
    return -1;

  stream->state     &= ~_STATE_EOF;
  stream->back_count = 0;
  stream->read_save  = NULL;
  stream->next       = NULL;
  stream->write_end  = NULL;
  stream->read_end   = NULL;

  return 0;
}
