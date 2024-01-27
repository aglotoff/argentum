#include <netdb.h>
#include <stdio.h>

struct hostent *
gethostbyname(const char *name)
{
  fprintf(stderr, "TODO: gethostbyname(%s)\n", name);
  return NULL;
}
