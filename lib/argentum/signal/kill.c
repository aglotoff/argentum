#include <signal.h>
#include <sys/syscall.h>

int
_kill(pid_t pid, int sig)
{
  return __syscall(__SYS_KILL, pid, sig, 0, 0, 0, 0);
}

int
kill(pid_t pid, int sig)
{
  return _kill(pid, sig);
}
