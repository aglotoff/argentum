#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
access(const char *path, int amode)
{
  (void) path;
  (void) amode;

  fprintf(stderr, "TODO: access\n");
  abort();

  return -1;
}
