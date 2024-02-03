#include <unistd.h>
#include <sys/syscall.h>

pid_t
getppid(void)
{
  return __syscall(__SYS_GETPPID, 0, 0, 0, 0, 0, 0);
}
