#include <signal.h>
#include <sys/syscall.h>

int
_kill(pid_t pid, int sig)
{
  return __syscall2(__SYS_KILL, pid, sig);
}

int
kill(pid_t pid, int sig)
{
  return _kill(pid, sig);
}
