#ifndef _ARPA_INET_H
# define _ARPA_INET_H

#include <netinet/in.h>

#define htonl(hostlong)   ((uint32_t)                \
  ((((hostlong) & (uint32_t) 0x000000FFUL) << 24) |  \
   (((hostlong) & (uint32_t) 0x0000FF00UL) <<  8) |  \
   (((hostlong) & (uint32_t) 0x00FF0000UL) >>  8) |  \
   (((hostlong) & (uint32_t) 0xFF000000UL) >> 24)))

#define htons(hostshort)  ((uint16_t)               \
  ((((hostshort) & (uint16_t) 0x000000FFU) << 8) |  \
   (((hostshort) & (uint16_t) 0x0000FF00U) >> 8)))

#define ntohl(netlong)  htonl(netlong)
#define ntohs(netshort) htons(netshort)

int inet_pton(int, const char *, void *);

#endif
