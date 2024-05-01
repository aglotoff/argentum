#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
wait(int *stat_loc)
{
  return __syscall2(__SYS_WAIT, -1, stat_loc);
}
