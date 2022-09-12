#include <stdio.h>

FILE *
freopen(const char *pathname, const char *mode, FILE *stream)
{
  __fclose(stream);

  stream->mode &= _MODE_ALLOC_FILE;

  if (__fopen(stream, pathname, mode) != 0) {
    __ffree(stream);
    return NULL;
  }

  return stream;
}
