#include <termios.h>

int
cfsetospeed(struct termios *termios_p, speed_t speed)
{
  termios_p->c_ospeed = speed;
  return 0;
}
