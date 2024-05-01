#include <sys/syscall.h>
#include <unistd.h>

pid_t
fork(void)
{
  return __syscall0(__SYS_FORK);
}
