#include <sys/ioctl.h>
#include <unistd.h>

pid_t
tcgetpgrp(int fildes)
{
  return ioctl(fildes, TIOCGPGRP);
}
