#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int
raise(int sig)
{
  (void) sig;

  // fprintf(stderr, "TODO: raise\n");

  return -1;
}
