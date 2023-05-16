#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
dup(int fildes)
{
  (void) fildes;

  fprintf(stderr, "TODO: dup");
  abort();

  return -1;
}
