#include <stdio.h>
#include <unistd.h>

int
setuid(uid_t uid)
{
  fprintf(stderr, "TODO: setuid(%d)\n", uid);
  return uid == 0 ? 0 : -1;
}
