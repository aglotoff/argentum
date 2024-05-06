#include <termios.h>

speed_t
cfgetospeed(const struct termios *termios_p)
{
  return termios_p->c_ospeed;
}
