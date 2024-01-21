#include <termios.h>
#include <stdio.h>

int
tcgetattr(int fildes, struct termios *termios_p)
{
  fprintf("TODO: tcgetattr(%d, %p)\n", fildes, termios_p);
  return -1;
}
