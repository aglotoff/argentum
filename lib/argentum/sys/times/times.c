#include <sys/times.h>
#include <sys/syscall.h>

clock_t
_times(struct tms *buffer)
{
  return __syscall1(__SYS_TIMES, buffer);
}

clock_t
times(struct tms *buffer)
{
  return _times(buffer);
}
