#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>

int
getrusage(int who, struct rusage *usage)
{
  fprintf(stderr, "TODO: getrusage(%d,%p)\n", who, usage);
  return -1;
}
