#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int
clock_getres(clockid_t clock_id, struct timespec *res)
{
  fprintf(stderr, "TODO: clock_getres(%d,%p)\n", clock_id, res);
  return -1;
}
