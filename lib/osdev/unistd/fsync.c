#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int 
_fsync(int fd)
{
  (void) fd;

  fprintf(stderr, "TODO: fsync()\n");
  // abort();

  return 0;
}

int
fsync(int fd)
{
  return _fsync(fd);
}
