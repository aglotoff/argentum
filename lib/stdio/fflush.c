#include <stdio.h>
#include <stdlib.h>

int
fflush(FILE *stream)
{
  (void) stream;

  fprintf(stderr, "TODO: fflush");
  abort();

  return -1;
}
