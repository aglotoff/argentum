#include <sys/ioctl.h>
#include <unistd.h>

int
tcsetpgrp(int fildes, pid_t pgid_id)
{
  return ioctl(fildes, TIOCSPGRP, pgid_id);
}
