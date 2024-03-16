#include <stdio.h>
#include <sys/time.h>

int
setitimer(int which, const struct itimerval *value,
          struct itimerval *ovalue)
{
  (void) which;
  (void) value;
  (void) ovalue;

  fprintf(stderr, "TODO: setitimer\n");
  return -1;
}
