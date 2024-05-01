#include <signal.h>
#include <sys/syscall.h>

// Defined in sigstub.S
void __sigstub(void *);

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  return __syscall4(__SYS_SIGACTION, sig, __sigstub, act, oact);
}
