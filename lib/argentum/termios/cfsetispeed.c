#include <termios.h>
#include <stdio.h>

int
cfsetispeed(struct termios *termios_p, speed_t speed)
{
  (void) speed;
  fprintf(stderr, "TODO: cfsetispeed(%p)\n", termios_p);
  return -1;
}
