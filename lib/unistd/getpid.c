#include <syscall.h>
#include <unistd.h>

pid_t
getpid(void)
{
  return syscall(SYS_getpid, 0, 0, 0);
}
