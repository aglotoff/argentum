#ifndef _ARPA_INET_H
# define _ARPA_INET_H

#include <inttypes.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};

#define INET_ADDRSTRLEN   16
#define INET6_ADDRSTRLEN  46

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

in_addr_t inet_addr(const char *);
int		    inet_aton(const char *, struct in_addr *);
char     *inet_ntoa(struct in_addr);
int       inet_pton(int, const char *, void *);

#endif
