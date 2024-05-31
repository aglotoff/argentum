#include <arpa/inet.h>
#include <stdio.h>

#define BUF_MAX 16  // "255.255.255.255\0"

static char buf[BUF_MAX];

char *
inet_ntoa(struct in_addr in)
{
  in_addr_t addr = ntohl(in.s_addr);

  snprintf(buf, BUF_MAX, "%d.%d.%d.%d",
           (addr >> 24) & 0xFF,
           (addr >> 16) & 0xFF,
           (addr >> 8) & 0xFF,
           addr & 0xFF);

  return buf;
}
