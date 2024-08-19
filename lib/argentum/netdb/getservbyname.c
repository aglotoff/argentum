#include <netdb.h>
#include <stdio.h>

struct servent *
getservbyname(const char *name, const char *proto)
{
  struct servent *p;

  while ((p = getservent()) != NULL) {
    if ((strcmp(name, p->s_name) == 0) && (strcmp(proto, p->s_proto) == 0))
      return p;
  }
  return NULL;
}
