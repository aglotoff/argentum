#include <signal.h>
#include <stdio.h>

int sigsuspend
(const sigset_t *sigmask)
{
  fprintf(stderr, "TODO: sigsuspend(%p)\n", sigmask);
  return -1;
}
