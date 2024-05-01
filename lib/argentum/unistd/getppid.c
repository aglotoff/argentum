#include <unistd.h>
#include <sys/syscall.h>

pid_t
getppid(void)
{
  return __syscall0(__SYS_GETPPID);
}
