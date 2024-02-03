#include <stdio.h>
#include <unistd.h>

long
fpathconf(int fildes, int name)
{
  fprintf(stderr, "TODO: fpathconf(%d,%d)\n", fildes, name);
  return -1;
}
