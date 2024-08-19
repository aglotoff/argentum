#include <netdb.h>
#include <stdio.h>

struct protoent *
getprotobyname(const char *name)
{
  struct protoent *p;

  while ((p = getprotoent()) != NULL) {
    if (strcmp(name, p->p_name) == 0)
      return p;
  }
  return NULL;
}
