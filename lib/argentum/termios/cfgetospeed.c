#include <termios.h>
#include <stdio.h>

speed_t
cfgetospeed(const struct termios *termios_p)
{
  fprintf("TODO: cfgetospeed(%p)\n", termios_p);
  return -1;
}
