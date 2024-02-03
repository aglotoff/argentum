#include <unistd.h>
#include <stdio.h>

int
setregid(gid_t rgid, gid_t egid)
{
  fprintf(stderr, "TODO: setregid(%d,%d)\n", rgid, egid);
  return -1;
}
