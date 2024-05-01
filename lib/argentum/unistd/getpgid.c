#include <unistd.h>
#include <sys/syscall.h>

pid_t
getpgid(pid_t pid)
{
  return __syscall1(__SYS_GETPGID, pid);
}
