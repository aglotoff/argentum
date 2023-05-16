#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int
fcntl(int fildes, int cmd, ...)
{
  (void) fildes;
  (void) cmd;

  fprintf(stderr, "TODO: fcntl");
  abort();

  return -1;
}
