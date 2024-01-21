#include <termios.h>
#include <stdio.h>

int
tcsetattr(int fildes, int optional_actions, const struct termios *termios_p)
{
  fprintf("TODO: tcsetattr(%d,%x,%p)\n", fildes, optional_actions, termios_p);
  return -1;
}
