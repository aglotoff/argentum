#include <unistd.h>
#include <sys/syscall.h>

int
setpgid(pid_t pid, pid_t pgid)
{
  return __syscall(__SYS_SETPGID, pid, pgid, 0, 0, 0, 0);
}
