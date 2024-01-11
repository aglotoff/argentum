#include <signal.h>
#include <sys/syscall.h>

void __sigstub(void *);

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  return __syscall(__SYS_SIGACTION, sig, (uint32_t) __sigstub, (uint32_t) act, (uint32_t) oact, 0, 0);
}
