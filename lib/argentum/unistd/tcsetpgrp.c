#include <sys/ioctl.h>
#include <sys/ttycom.h>
#include <unistd.h>

int
tcsetpgrp(int fildes, pid_t pgid_id)
{
  return ioctl(fildes, TCSETPGRP, pgid_id);
}
