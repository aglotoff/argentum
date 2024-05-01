#include <unistd.h>
#include <sys/syscall.h>

int
setpgid(pid_t pid, pid_t pgid)
{
  return __syscall2(__SYS_SETPGID, pid, pgid);
}
