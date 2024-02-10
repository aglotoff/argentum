#include <netdb.h>
#include <stdio.h>

struct hostent *
gethostbyaddr(const void *addr, socklen_t len, int type)
{
  (void) addr;
  (void) len;
  (void) type;

  fprintf(stderr, "TODO: gethostbyaddr()\n");
  return NULL;
}
