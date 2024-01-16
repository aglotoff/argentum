#include <sys/times.h>
#include <stdlib.h>
#include <stdio.h>

clock_t
_times(struct tms *buffer)
{
  (void) buffer;

  fprintf(stderr, "TODO: times\n");

  return -1;
}

clock_t
times(struct tms *buffer)
{
  return _times(buffer);
}
