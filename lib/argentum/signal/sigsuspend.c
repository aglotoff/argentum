#include <signal.h>
#include <sys/syscall.h>

int
sigsuspend(const sigset_t *sigmask)
{
  return __syscall1(__SYS_SIGSUSPEND, sigmask);
}
