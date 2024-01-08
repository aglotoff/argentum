#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
wait(int *stat_loc)
{
  return __syscall(__SYS_WAIT, -1, (uint32_t) stat_loc, 0, 0, 0, 0);
}
