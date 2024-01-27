#include <stdio.h>
#include <unistd.h>

int
sethostname(const char *name, size_t namelen)
{
  fprintf(stderr, "TODO: sethostname(%s,%d)\n", name, namelen);
  return -1;
}
