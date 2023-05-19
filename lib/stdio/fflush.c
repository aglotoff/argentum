#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
fflush(FILE *stream)
{
  if (stream->write_end != NULL)
    return __fflush(stream);

  return 0;
}
