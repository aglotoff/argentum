#ifndef _NETDB_H
# define _NETDB_H

#include <netinet/in.h>

struct hostent {
  char   *h_name;
  char  **h_aliases;
  int     h_addrtype;
  int     h_length; 
  char  **h_addr_list;
#define	h_addr	h_addr_list[0]
};

struct servent {
  char   *s_name;
  char  **s_aliases;
  int     s_port;
  char   *s_proto;
};

struct protoent {
  char   *p_name;
  char  **p_aliases;
  int     p_proto;
};

#define	NI_NOFQDN	      0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	    0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	      0x00000010
#define	NI_NUMERICSCOPE	0x00000020

struct hostent  *gethostbyaddr(const void *, socklen_t, int);
struct hostent  *gethostbyname(const char *);
struct protoent *getprotobyname(const char *);
struct servent  *getservbyname(const char *, const char *);

extern int h_errno;

#endif
