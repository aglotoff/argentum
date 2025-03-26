#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
setgroups(int ngroups, const gid_t *grouplist)
{
  fprintf(stderr, "TODO: setgroups(%d, %p)\n",ngroups, grouplist);
  return -1;
}
