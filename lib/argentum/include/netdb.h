#ifndef _NETDB_H
# define _NETDB_H

#include <sys/cdefs.h>

__BEGIN_DECLS

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

#ifndef __ARGENTUM_KERNEL__

#include <netinet/in.h>

#ifndef _PATH_PROTOCOLS
#define _PATH_PROTOCOLS "/etc/protocols"
#endif

#ifndef _PATH_SERVICES
#define _PATH_SERVICES "/etc/services"
#endif

void             endservent(void);
struct protoent *getprotoent(void);
struct servent  *getservent(void);
struct hostent  *gethostbyaddr(const void *, socklen_t, int);
struct hostent  *gethostbyname(const char *);
struct protoent *getprotobyname(const char *);
struct servent  *getservbyname(const char *, const char *);
void             setservent(int);

extern int h_errno;

#endif  // !__ARGENTUM_KERNEL__

__END_DECLS

#endif
