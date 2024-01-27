#include <stdio.h>
#include <unistd.h>

int
ftruncate(int fildes, off_t length)
{
  fprintf(stderr, "TODO: ftruncate(%d,%d)\n", fildes, length);
  return -1;
}
