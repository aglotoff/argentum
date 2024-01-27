#include <stdio.h>
#include <unistd.h>

long
pathconf(const char *path, int name)
{
  fprintf(stderr, "TODO: pathconf(%s,%d)\n", path, name);
  return -1;
}
