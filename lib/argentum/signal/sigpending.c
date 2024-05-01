#include <signal.h>
#include <sys/syscall.h>

int
sigpending(sigset_t *set)
{
  return __syscall1(__SYS_SIGPENDING, set);
}
