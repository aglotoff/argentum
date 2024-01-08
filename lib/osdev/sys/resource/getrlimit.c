#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

int
getrlimit(int resource, struct rlimit *rlim)
{
  (void) resource;
  (void) rlim;

  fprintf(stderr, "TODO: getrlimit(%d)\n");
  // abort();

  return -1;
}
