#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
  (void) how;
  (void) set;
  (void) oset;

  fprintf(stderr, "TODO: sigprocmask\n");

  return 0;
}
