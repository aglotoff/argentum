#include <unistd.h>
#include <stdio.h>

long
sysconf(int name)
{
  (void) name;

  fprintf(stderr, "TODO: sysconf(%d)\n", name);
  return 20;
}
