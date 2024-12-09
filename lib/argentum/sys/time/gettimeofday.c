#include <sys/time.h>

int
_gettimeofday (struct timeval *tp, void *tzp)
{
  struct timespec time;
  int ret;

  // TODO: handle timezones
  (void) tzp;

  ret = clock_gettime(CLOCK_MONOTONIC, &time);

  if (ret == 0) {
    tp->tv_sec  = time.tv_sec;
    tp->tv_usec = time.tv_nsec / 1000;
  }

  return ret;
}

int
gettimeofday (struct timeval *tp, void *tzp)
{
  return _gettimeofday(tp, tzp);
}
