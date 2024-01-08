#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int
_kill(pid_t pid, int sig)
{
  (void) pid;
  (void) sig;

  fprintf(stderr, "TODO: kill\n");
 
  return -1;
}

int
kill(pid_t pid, int sig)
{
  return _kill(pid, sig);
}
