#include <stdio.h>
#include <sys/vfs.h>

int
statfs(const char *path, struct statfs *buf)
{
  fprintf(stderr, "TODO: statfs(%s,%p)\n", path, buf);
  return -1;
}
