#include <sys/syscall.h>
#include <unistd.h>

pid_t
_getpid(void)
{
  return __syscall(__SYS_GETPID, 0, 0, 0);
}

pid_t
getpid(void)
{
  return _getpid();
}
