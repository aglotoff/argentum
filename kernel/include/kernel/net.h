#ifndef __KERNEL_INCLUDE_KERNEL_NET_H__
#define __KERNEL_INCLUDE_KERNEL_NET_H__

#include <stddef.h>
#include <lwip/sockets.h>

struct File;

void net_enqueue(void *, size_t);
void net_init(void);

int     net_socket(int, int, int, struct File **);
int     net_bind(int, const struct sockaddr *, socklen_t);
int     net_listen(int, int);
int     net_connect(int, const struct sockaddr *, socklen_t);
int     net_accept(int, struct sockaddr *, socklen_t *, struct File **);
int     net_close(int);
ssize_t net_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t net_sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t );
int     net_setsockopt(int, int, int, const void *, socklen_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
