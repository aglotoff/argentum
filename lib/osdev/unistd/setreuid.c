#include <unistd.h>
#include <stdio.h>

int
setreuid(uid_t ruid, uid_t euid)
{
  fprintf(stderr, "TODO: setreuid(%d,%d)\n", ruid, euid);
  return -1;
}
