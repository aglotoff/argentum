#include <sys/syscall.h>
#include <sys/time.h>
#include <stdio.h>

int
getitimer(int which, struct itimerval *curr_value)
{
  fprintf(stderr, "TODO: setitimer\n");

  (void) which;
  (void) curr_value;

  return -1;
}
