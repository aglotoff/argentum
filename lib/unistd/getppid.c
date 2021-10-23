#include <syscall.h>
#include <unistd.h>

pid_t
getppid(void)
{
  return syscall(SYS_getppid, 0, 0, 0);
}
