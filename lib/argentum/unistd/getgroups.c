#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
getgroups(int gidsetsize, gid_t grouplist[])
{
  fprintf(stderr, "TODO: getgroups(%d, %p)\n", gidsetsize, grouplist);
  return -1;
}
