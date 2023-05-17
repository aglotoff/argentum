#include <errno.h>
#include <signal.h>

int
sigaddset(sigset_t *set, int signo)
{
  if ((signo <= 0) || (signo > SIGXFSZ)) {
    errno = EINVAL;
    return -1;
  }

  *set |= (1UL << signo);

  return 0;
}
