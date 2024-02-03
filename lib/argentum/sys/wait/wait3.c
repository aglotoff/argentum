#include <sys/syscall.h>
#include <sys/wait.h>

pid_t
wait3(int *stat_loc, int options, struct rusage *rusage)
{
  return __syscall(__SYS_WAIT, -1, (uint32_t) stat_loc, options,
                   (uintptr_t) rusage, 0, 0);
}
