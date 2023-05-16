#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
access(const char *path, int amode)
{
  (void) path;
  (void) amode;

  fprintf(stderr, "TODO: access");
  abort();

  return -1;
}
