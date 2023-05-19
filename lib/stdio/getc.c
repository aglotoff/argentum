#include <stdio.h>
#include <stdlib.h>

int
(getc)(FILE *stream)
{
  return fgetc(stream);
}
