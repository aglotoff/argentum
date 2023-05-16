#include <stdio.h>
#include <stdlib.h>

int
ungetc(int c, FILE *stream)
{
  (void) c;
  (void) stream;

  fprintf(stderr, "TODO: ungetc");
  abort();

  return -1;
}
