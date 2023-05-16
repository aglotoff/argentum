#include <stdio.h>
#include <stdlib.h>

int
fsync(int fildes)
{
  (void) fildes;

  fprintf(stderr, "TODO: fsync");
  abort();

  return -1;
}
