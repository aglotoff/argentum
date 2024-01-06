#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
  return __syscall(__SYS_WAIT, pid, (uint32_t) stat_loc, options);
}
