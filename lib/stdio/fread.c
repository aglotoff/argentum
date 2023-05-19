#include <stdio.h>

size_t
fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
  unsigned char *buf = (unsigned char *) ptr;
  size_t n;

  if (size == 0)
    return 0;

  // TODO: manipulate the stream buffer directly instead of calling fgetc?
  for (n = 0; n < size * nitems; n++) {
    int c = fgetc(stream);

    if (c == EOF)
      break;

    *buf++ = c;
  }

  return n / size;
}
