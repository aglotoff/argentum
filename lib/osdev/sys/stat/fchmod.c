#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int
fchmod(int fildes, mode_t mode)
{
  (void) fildes;
  (void) mode;

  fprintf(stderr, "TODO: fchmod\n");
  abort();

  return -1;
}
