#include <sys/syscall.h>
#include <unistd.h>

pid_t
_getpid(void)
{
  return __syscall0(__SYS_GETPID);
}

pid_t
getpid(void)
{
  return _getpid();
}
