#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  (void) sig;
  (void) act;
  (void) oact;

  // fprintf(stderr, "TODO: sigaction\n");

  return 0;
}
