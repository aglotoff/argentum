#ifndef _SYS_SOCKET_H
# define _SYS_SOCKET_H

// Keep these values in sync with lwip!

#include <stdint.h>

typedef uint32_t socklen_t;
typedef uint8_t  sa_family_t;

#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

struct sockaddr {
  uint8_t     sa_len;
  sa_family_t sa_family;
  char        sa_data[14];
};

int accept(int, struct sockaddr *, socklen_t *);
int bind(int, const struct sockaddr *, socklen_t);
int connect(int, const struct sockaddr *, socklen_t);
int listen(int, int);
int socket(int, int, int);

#endif
