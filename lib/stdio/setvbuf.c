#include <stdio.h>
#include <stdlib.h>

int
setvbuf(FILE *stream, char *buf, int type, size_t size)
{
  (void) stream;
  (void) buf;
  (void) type;
  (void) size;

  fprintf(stderr, "TODO: setvbuf");
  abort();

  return -1;
}
