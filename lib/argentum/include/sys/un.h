#ifndef __SYS_UN_H__
#define __SYS_UN_H__

#include <sys/socket.h>

struct sockaddr_un {
  sa_family_t  sun_family;
  char         sun_path[104];
};

#endif  // !__SYS_UN_H__
