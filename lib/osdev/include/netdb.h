#ifndef _NETDB_H
# define _NETDB_H

#include <netinet/in.h>

struct hostent {
  char   *h_name;
  char  **h_aliases;
  int     h_addrtype;
  int     h_length; 
  char  **h_addr_list;
};

struct servent {
  char   *s_name;
  char  **s_aliases;
  int     s_port;
  char   *s_proto;
};

struct hostent *gethostbyname(const char *);
struct servent *getservbyname(const char *, const char *);

#endif
