#include <arpa/inet.h>
#include <stdio.h>

int
inet_aton(const char *cp, struct in_addr *inp)
{
  (void) cp;
  (void) inp;
  fprintf(stderr, "TODO: inet_aton()\n");
  return -1;
}
