#include <termios.h>
#include <stdio.h>

int
tcflush(int fildes, int queue_selector)
{
  fprintf("TODO: tcflush(%d, %d)\n", fildes, queue_selector);
  return -1;
}
