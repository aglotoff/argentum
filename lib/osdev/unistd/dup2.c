#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
dup2(int oldfd, int newfd)
{
  (void) oldfd;
  (void) newfd;

  fprintf(stderr, "TODO: dup2\n");
  abort();

  return -1;
}