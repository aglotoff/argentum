#include <sys/syscall.h>
#include <unistd.h>

pid_t
vfork(void)
{
  return __syscall(__SYS_FORK, 1, 0, 0, 0, 0, 0);
}
