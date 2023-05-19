#include <stdio.h>
#include <stdlib.h>

int
(putc)(int c, FILE *stream)
{
  return fputc(c, stream);
}
