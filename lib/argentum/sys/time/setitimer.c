#include <sys/syscall.h>
#include <sys/time.h>

int
setitimer(int which,
          const struct itimerval *value,
          struct itimerval *ovalue)
{
  return __syscall3(__SYS_SETITIMER, which, value, ovalue);
}
