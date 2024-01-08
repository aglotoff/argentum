#include <sys/syscall.h>
#include <unistd.h>

pid_t
fork(void)
{
  return __syscall(__SYS_FORK, 0, 0, 0, 0, 0, 0);
}
