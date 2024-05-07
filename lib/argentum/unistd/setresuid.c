#include <unistd.h>
#include <stdio.h>

int
setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
  fprintf(stderr, "TODO: setreuid(%d,%d,%d)\n", ruid, euid, suid);
  return -1;
}
