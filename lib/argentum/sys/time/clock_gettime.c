#include <sys/syscall.h>
#include <sys/time.h>

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  return __syscall2(__SYS_CLOCK_TIME, clock_id, tp);
}
