#include <stdio.h>
#include <unistd.h>

ssize_t
readlink(const char *path, char *buf, size_t bufsize)
{
  (void) path;
  (void) buf;
  (void) bufsize;

  fprintf(stderr, "TODO: readlink\n");
  return -1;
}
