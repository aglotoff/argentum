#ifndef __KERNEL_INCLUDE_KERNEL_NET_H__
#define __KERNEL_INCLUDE_KERNEL_NET_H__

#include <stddef.h>
#include <lwip/sockets.h>

struct Channel;

void net_enqueue(void *, size_t);
void net_init(void);

int     net_socket(int, int, int, struct Channel **);
int     net_accept(struct Channel *, struct sockaddr *, socklen_t *, struct Channel **);
int     net_close(struct Channel *);

int     net_bind(struct Channel *, const struct sockaddr *, socklen_t);
int     net_listen(struct Channel *, int);
int     net_connect(struct Channel *, const struct sockaddr *, socklen_t);
ssize_t net_recvfrom(struct Channel *, uintptr_t, size_t, int, struct sockaddr *, socklen_t *);
ssize_t net_sendto(struct Channel *, uintptr_t, size_t, int, const struct sockaddr *, socklen_t );
int     net_setsockopt(struct Channel *, int, int, const void *, socklen_t);
ssize_t net_read(struct Channel *, uintptr_t, size_t);
ssize_t net_write(struct Channel *, uintptr_t, size_t);
int     net_select(struct Channel *, struct timeval *);
int     net_gethostbyname(const char *, ip_addr_t *);

intptr_t net_send_recv(struct Channel *, void *, size_t, void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
