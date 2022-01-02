#include <syscall.h>
#include <unistd.h>

pid_t
getppid(void)
{
  return __syscall(__SYS_GETPPID, 0, 0, 0);
}
