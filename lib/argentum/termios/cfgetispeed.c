#include <termios.h>

speed_t
cfgetispeed(const struct termios *termios_p)
{
  return termios_p->c_ispeed;
}
