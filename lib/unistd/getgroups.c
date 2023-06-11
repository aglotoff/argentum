#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
getgroups(int gidsetsize, gid_t grouplist[])
{
  (void) gidsetsize;
  (void) grouplist;

  fprintf(stderr, "TODO: getgroups\n");
  exit(EXIT_FAILURE);

  return 0;
}
