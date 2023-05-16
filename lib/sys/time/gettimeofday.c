#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

int
gettimeofday(struct timeval *tp, void *tzp)
{
  (void) tp;
  (void) tzp;

  fprintf(stderr, "TODO: gettimeofday");
  abort();

  return -1;
}
