#include <stdio.h>
#include <unistd.h>

int
getdomainname(char *name, size_t len)
{
  fprintf(stderr, "TODO: getdomainname(%.*s)\n", len, name);
  return -1;
}
