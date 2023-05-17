#include <errno.h>
#include <signal.h>

int
sigismember(const sigset_t *set, int signo)
{
  if ((signo <= 0) || (signo > SIGXFSZ)) {
    errno = EINVAL;
    return -1;
  }

  return (*set & (1UL << signo)) != 0;
}
