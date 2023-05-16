#include <stdio.h>
#include <stdlib.h>

size_t
fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
  (void) ptr;
  (void) size;
  (void) nitems;
  (void) stream;

  fprintf(stderr, "TODO: fwrite");
  abort();

  return -1;
}
