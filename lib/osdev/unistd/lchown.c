#include <stdio.h>
#include <unistd.h>

int
lchown(const char *path, uid_t owner, gid_t group)
{
  fprintf(stderr, "TODO: lchown(%s,%d,%d)\n", path, owner, group);
  return -1;
}
