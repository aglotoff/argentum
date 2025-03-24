#include <stdio.h>
#include <stdlib.h>
#include <grp.h>

int
initgroups(const char *user, gid_t group)
{
  fprintf(stderr, "TODO: initgroups(%s, %d)\n", user, group);
  abort();
  return -1;
}
