#include <sys/syscall.h>
#include <sys/time.h>

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  return __syscall2(__SYS_CLOCK_TIME, clock_id, tp);
}

int
_gettimeofday (struct timeval *tp, void *tzp)
{
  struct timespec t;
  int ret;

  (void) tzp;

  // TODO: replace by CLOCK_MONOTONIC
  ret = clock_gettime(CLOCK_REALTIME, &t);

  if (ret == 0) {
    tp->tv_sec  = t.tv_sec;
    tp->tv_usec = t.tv_nsec / 1000;
  }

  return ret;
}

int
gettimeofday (struct timeval *tp, void *tzp)
{
  return _gettimeofday(tp, tzp);
}
