#include <sys/ioctl.h>
#include <termios.h>

int
tcgetattr(int fildes, struct termios *termios_p)
{
  return ioctl(fildes, TIOCGETA, termios_p);
}
