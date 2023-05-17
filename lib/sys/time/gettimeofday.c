#include <sys/time.h>
#include <time.h>

int
gettimeofday(struct timeval *tp, void *tzp)
{
  (void) tzp;

  tp->tv_sec  = time(NULL);
  tp->tv_usec = 0;

  // TODO: use timezone info
  // TODO: add microseconds

  return -1;
}
