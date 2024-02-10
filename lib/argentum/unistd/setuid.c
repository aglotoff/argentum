#include <stdio.h>
#include <unistd.h>

int
setuid(uid_t uid)
{
  fprintf(stderr, "TODO: setuid(%d)\n", uid);
  return -1;
}
