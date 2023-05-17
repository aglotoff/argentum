#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int
sigemptyset(sigset_t *set)
{
  *set = 0;

  return 0;
}
