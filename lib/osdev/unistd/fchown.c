#include <stdio.h>
#include <unistd.h>

int
fchown(int fildes, uid_t owner, gid_t group)
{
  fprintf(stderr, "TODO: fchown(%d,%d,%d)\n", fildes, owner, group);
  return -1;
}
