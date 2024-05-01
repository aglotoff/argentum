#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
  return __syscall3(__SYS_WAIT, pid, stat_loc, options);
}
