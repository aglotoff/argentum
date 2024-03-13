#ifndef _SYS_SOCKET_H
# define _SYS_SOCKET_H

// Keep these values in sync with lwip!

#include <stdint.h>
#include <sys/types.h>

typedef uint32_t socklen_t;
typedef uint8_t  sa_family_t;

#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_INET         2
#define AF_INET6        10
#define PF_UNSPEC       AF_UNSPEC
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define SOL_SOCKET     0xfff

#define SO_DEBUG        0x0001 /* Unimplemented: turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002 /* socket has had listen() */
#define SO_DONTROUTE    0x0010 /* Unimplemented: just use interface addresses */
#define SO_USELOOPBACK  0x0040 /* Unimplemented: bypass hardware when possible */
#define SO_LINGER       0x0080 /* linger on close if data present */
#define SO_DONTLINGER   ((int)(~SO_LINGER))
#define SO_OOBINLINE    0x0100 /* Unimplemented: leave received OOB data in line */
#define SO_REUSEPORT    0x0200 /* Unimplemented: allow local address & port reuse */
#define SO_SNDBUF       0x1001 /* Unimplemented: send buffer size */
#define SO_RCVBUF       0x1002 /* receive buffer size */
#define SO_SNDLOWAT     0x1003 /* Unimplemented: send low-water mark */
#define SO_RCVLOWAT     0x1004 /* Unimplemented: receive low-water mark */
#define SO_SNDTIMEO     0x1005 /* send timeout */
#define SO_RCVTIMEO     0x1006 /* receive timeout */
#define SO_ERROR        0x1007 /* get error status and clear */
#define SO_TYPE         0x1008 /* get socket type */

#define SO_REUSEADDR   0x0004 /* Allow local address reuse */
#define SO_KEEPALIVE   0x0008 /* keep connections alive */
#define SO_BROADCAST   0x0020 /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */

#define MSG_PEEK        0x01
#define MSG_WAITALL     0x02
#define MSG_OOB         0x04
#define MSG_DONTWAIT    0x08
#define MSG_MORE        0x10
#define MSG_NOSIGNAL    0x20

struct sockaddr {
  uint8_t     sa_len;
  sa_family_t sa_family;
  char        sa_data[14];
};

int     accept(int, struct sockaddr *, socklen_t *);
int     bind(int, const struct sockaddr *, socklen_t);
int     connect(int, const struct sockaddr *, socklen_t);
int     getsockname(int, struct sockaddr *,  socklen_t *);
int     listen(int, int);
int     socket(int, int, int);
int     setsockopt(int, int, int, const void *, socklen_t);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *dest_addr,
        socklen_t);
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t recv(int, void *, size_t, int);
ssize_t send(int, const void *, size_t, int);
int     shutdown(int, int);

#endif
