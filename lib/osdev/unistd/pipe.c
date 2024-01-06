#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
pipe(int fildes[2])
{
  (void) fildes;

  fprintf(stderr, "TODO: pipe\n");
  abort();

  return -1;
}
