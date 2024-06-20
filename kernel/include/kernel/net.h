#ifndef __KERNEL_INCLUDE_KERNEL_NET_H__
#define __KERNEL_INCLUDE_KERNEL_NET_H__

#include <stddef.h>
#include <lwip/sockets.h>

struct File;

void net_enqueue(void *, size_t);
void net_init(void);

int     net_socket(int, int, int, struct File **);
int     net_accept(struct File *, struct sockaddr *, socklen_t *, struct File **);
int     net_close(struct File *);

int     net_bind(struct File *, const struct sockaddr *, socklen_t);
int     net_listen(struct File *, int);
int     net_connect(struct File *, const struct sockaddr *, socklen_t);
ssize_t net_recvfrom(struct File *, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t net_sendto(struct File *, const void *, size_t, int, const struct sockaddr *, socklen_t );
int     net_setsockopt(struct File *, int, int, const void *, socklen_t);
ssize_t net_read(struct File *, void *, size_t);
ssize_t net_write(struct File *, const void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
