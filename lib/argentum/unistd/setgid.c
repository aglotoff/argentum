#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

int
setgid(gid_t gid)
{
  fprintf(stderr, "TODO: setgid(%d)\n", gid);
  return -1;
}
