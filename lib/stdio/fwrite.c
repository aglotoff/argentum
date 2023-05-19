#include <stdio.h>

size_t
fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
  const unsigned char *buf = (unsigned char *) ptr;
  size_t n;

  if (size == 0)
    return 0;

  // TODO: manipulate the stream buffer directly instead of calling fputc?
  for (n = 0; n < size * nitems; n++)
    if (fputc(*buf++, stream) == EOF)
      break;

  return n / size;
}
