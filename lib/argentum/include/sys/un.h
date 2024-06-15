#ifndef __SYS_UN_H__
#define __SYS_UN_H__

#include <sys/cdefs.h>
#include <sys/socket.h>

__BEGIN_DECLS

struct sockaddr_un {
  sa_family_t  sun_family;
  char         sun_path[104];
};

__END_DECLS

#endif  // !__SYS_UN_H__
