#include <grp.h>
#include <stdio.h>

struct group *
getgrgid(gid_t gid)
{
  fprintf(stderr, "TODO: getgrgid(%d)\n", gid);
  return -1;
}
