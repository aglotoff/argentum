#ifndef _SYS_NETINET_IN_H
# define _SYS_NETINET_IN_H

#include <inttypes.h>
#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};

struct sockaddr_in {
  uint8_t         sin_len;
  sa_family_t     sin_family;
  in_port_t       sin_port;
  struct in_addr  sin_addr;
#define SIN_ZERO_LEN 8
  char            sin_zero[SIN_ZERO_LEN];
};

#define INADDR_NONE         ((in_addr_t) 0xFFFFFFFFUL)
#define INADDR_LOOPBACK     ((in_addr_t) 0x7F000001UL)
#define INADDR_ANY          ((in_addr_t) 0x00000000UL)
#define INADDR_BROADCAST    ((in_addr_t) 0xFFFFFFFFUL)

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_IPV6    41
#define IPPROTO_RAW     255

#endif
