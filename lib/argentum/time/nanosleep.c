#include <sys/syscall.h>
#include <time.h>

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
  __syscall(__SYS_NANOSLEEP, (uint32_t) rqtp, (uint32_t) rmtp, 0, 0, 0, 0);
}
