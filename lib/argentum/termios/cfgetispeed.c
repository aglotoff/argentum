#include <termios.h>
#include <stdio.h>

speed_t
cfgetispeed(const struct termios *termios_p)
{
  fprintf("TODO: cfgetispeed(%p)\n", termios_p);
  return -1;
}
