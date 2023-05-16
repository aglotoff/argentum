#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
pipe(int fildes[2])
{
  (void) fildes;

  fprintf(stderr, "TODO: pipe");
  abort();

  return -1;
}
