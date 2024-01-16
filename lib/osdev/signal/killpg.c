#include <errno.h>
#include <signal.h>

int
killpg(pid_t pid, int sig)
{
  if (pid < 0) {
    errno = EINVAL;
    return -1;
  }
  return kill(-pid, sig);
}
