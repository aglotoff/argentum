#include <sys/ioctl.h>
#include <sys/ttycom.h>
#include <unistd.h>

pid_t
tcgetpgrp(int fildes)
{
  return ioctl(fildes, TCGETPGRP);
}
