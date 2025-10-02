#ifndef __KERNEL_INCLUDE_KERNEL_NET_H__
#define __KERNEL_INCLUDE_KERNEL_NET_H__

#include <stddef.h>
#include <lwip/sockets.h>

struct Connection;

void net_enqueue(void *, size_t);
void net_init(void);

int     net_socket(int, int, int, struct Connection **);
int     net_accept(struct Connection *, struct sockaddr *, socklen_t *, struct Connection **);
int     net_close(struct Connection *);

int     net_bind(struct Connection *, const struct sockaddr *, socklen_t);
int     net_listen(struct Connection *, int);
int     net_connect(struct Connection *, const struct sockaddr *, socklen_t);
ssize_t net_recvfrom(struct Connection *, uintptr_t, size_t, int, struct sockaddr *, socklen_t *);
ssize_t net_sendto(struct Connection *, uintptr_t, size_t, int, const struct sockaddr *, socklen_t );
int     net_setsockopt(struct Connection *, int, int, const void *, socklen_t);
ssize_t net_read(struct Connection *, uintptr_t, size_t);
ssize_t net_write(struct Connection *, uintptr_t, size_t);
int     net_select(struct Connection *, struct timeval *);
int     net_gethostbyname(const char *, ip_addr_t *);

intptr_t net_send_recv(struct Connection *, void *, size_t, void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
