#include <stdio.h>
#include <stdlib.h>

int
putc(int c, FILE *stream)
{
  fprintf(stream, "%c", c);
  return 0;
}
