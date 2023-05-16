#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
isatty(int fildes)
{
  (void) fildes;

  fprintf(stderr, "TODO: isatty");
  abort();

  return -1;
}
