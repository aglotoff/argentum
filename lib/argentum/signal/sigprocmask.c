#include <signal.h>
#include <sys/syscall.h>

int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
  return __syscall3(__SYS_SIGPROCMASK, how, set, oset);
}
