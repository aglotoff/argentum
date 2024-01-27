#include <netdb.h>
#include <stdio.h>

struct servent *
getservbyname(const char *name, const char *proto)
{
  fprintf(stderr, "TODO: getservbyname(%s,%s)\n", name, proto);
  return NULL;
}
