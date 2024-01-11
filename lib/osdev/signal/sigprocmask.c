#include <signal.h>
#include <sys/syscall.h>

int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
  return __syscall(__SYS_SIGPROCMASK, how, (uint32_t) set, (uint32_t) oset, 0, 0, 0);
}
