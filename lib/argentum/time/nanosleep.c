#include <sys/syscall.h>
#include <time.h>

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
  __syscall2(__SYS_NANOSLEEP, rqtp, rmtp);
}
