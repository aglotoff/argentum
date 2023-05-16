#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

off_t
lseek(int fildes, off_t offset, int whence)
{
  (void) fildes;
  (void) offset;
  (void) whence;

  fprintf(stderr, "TODO: lseek");
  abort();

  return -1;
}
