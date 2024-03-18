#include <arpa/inet.h>
#include <stdio.h>

in_addr_t
inet_addr(const char *cp)
{
  struct in_addr in;
  if (inet_aton(cp, &in) != 1)
    return (in_addr_t) -1;
  return in.s_addr;
}
