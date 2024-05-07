#include <unistd.h>
#include <stdio.h>

int
setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
  fprintf(stderr, "TODO: setregid(%d,%d,%d)\n", rgid, egid, sgid);
  return -1;
}
