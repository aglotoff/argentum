#include <stdio.h>
#include <stdlib.h>

int
ungetc(int c, FILE *stream)
{
  if (c == EOF)
    return EOF;
  if (!(stream->mode & _MODE_WRITE) || (stream->write_end != NULL))
    return EOF;
  if (stream->back_count >= _UNGETC_MAX)
    return EOF;

  stream->back[stream->back_count++] = c;

  stream->read_save = stream->read_end;
  stream->read_end  = NULL;

  return -1;
}
