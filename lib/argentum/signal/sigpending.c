#include <signal.h>
#include <stdio.h>

int
sigpending(sigset_t *set)
{
  fprintf(stderr, "TODO: sigpending(%p)\n", set);
  return -1;
}
