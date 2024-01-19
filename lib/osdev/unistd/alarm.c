#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned
alarm(unsigned int seconds)
{
  (void) seconds;

  fprintf(stderr, "TODO: alarm\n");
  // abort();

  return -1;
}
