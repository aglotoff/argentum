#include <stdio.h>

int
ferror(FILE *stream)
{
  return stream->state & _STATE_ERROR;
}
