#include <stdio.h>
#include <stdlib.h>

int
putc(int c, FILE *stream)
{
  (void) c;
  (void) stream;

  fprintf(stderr, "TODO: putc");
  abort();

  return -1;
}
