#include <syscall.h>
#include <unistd.h>

pid_t
getpid(void)
{
  return __syscall(__SYS_GETPID, 0, 0, 0);
}
