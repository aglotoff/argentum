#include <errno.h>
#include <stdio.h>
#include <sys/resource.h>

int
setrlimit(int resource, const struct rlimit *rlim)
{
  fprintf(stderr, "TODO: setrlimit(%d, %p)\n", resource, rlim);
  return -1;
}
