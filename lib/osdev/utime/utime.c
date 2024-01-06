#include <stdio.h>
#include <stdlib.h>
#include <utime.h>

int
utime(const char *path, const struct utimbuf *times)
{
  (void) path;
  (void) times;
  
  fprintf(stderr, "TODO: utime\n");

  return 0;
}
