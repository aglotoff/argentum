#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
wait3(int *stat_loc, int options, struct rusage *rusage)
{
  return __syscall4(__SYS_WAIT, -1, stat_loc, options, rusage);
}
