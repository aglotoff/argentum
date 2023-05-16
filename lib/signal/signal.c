#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void (*signal(int sig, void (*func)(int)))(int)
{
  (void) sig;
  (void) func;

  fprintf(stderr, "TODO: signal");
  abort();

  return (void (*)(int)) -1;
}
