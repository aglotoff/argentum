#ifndef _SYS_NETINET_IN_H
# define _SYS_NETINET_IN_H

#include <arpa/inet.h>
#include <sys/socket.h>

struct sockaddr_in {
  uint8_t         sin_len;
  sa_family_t     sin_family;
  in_port_t       sin_port;
  struct in_addr  sin_addr;
#define SIN_ZERO_LEN 8
  char            sin_zero[SIN_ZERO_LEN];
};

struct in6_addr {
  union {
    uint32_t u32_addr[4];
    uint8_t  u8_addr[16];
  } un;
#define s6_addr  un.u8_addr
};

struct sockaddr_in6 {
  uint8_t         sin6_len;      /* length of this structure    */
  sa_family_t     sin6_family;   /* AF_INET6                    */
  in_port_t       sin6_port;     /* Transport layer port #      */
  uint32_t        sin6_flowinfo; /* IPv6 flow information       */
  struct in6_addr sin6_addr;     /* IPv6 address                */
  uint32_t        sin6_scope_id; /* Set of interfaces for scope */
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

#define IP_TOS          1
#define IP_TTL          2

#define IPPORT_RESERVED 1024

#endif
