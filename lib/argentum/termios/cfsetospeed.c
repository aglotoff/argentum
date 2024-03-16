#include <termios.h>
#include <stdio.h>

int
cfsetospeed(struct termios *termios_p, speed_t speed)
{
  (void) speed;
  fprintf(stderr, "TODO: cfsetospeed(%p)\n", termios_p);
  return -1;
}
