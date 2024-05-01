#include <sys/syscall.h>
#include <unistd.h>

pid_t
vfork(void)
{
  return __syscall1(__SYS_FORK, 1);
}
