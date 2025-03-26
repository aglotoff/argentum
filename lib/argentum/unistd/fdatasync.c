#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
fdatasync(int fildes)
{
  fprintf(stderr, "TODO: fdatasync(%d)\n", fildes);
  return -1;
}
