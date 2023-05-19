#include <stdio.h>

int
feof(FILE *stream)
{
  return stream->state & _STATE_EOF;
}
