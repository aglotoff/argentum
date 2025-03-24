#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int
sigwait(const sigset_t *set, int *sig)
{
  (void) set;
  (void) sig;

  fprintf(stderr, "TODO: sigwait\n");
  abort();

  return -1;
}
