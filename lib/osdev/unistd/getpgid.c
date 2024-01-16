#include <unistd.h>
#include <sys/syscall.h>

pid_t
getpgid(pid_t pid)
{
  return __syscall(__SYS_GETPGID, pid, 0, 0, 0, 0, 0);
}
