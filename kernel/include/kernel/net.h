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
ssize_t net_recvfrom(struct File *, uintptr_t, size_t, int, struct sockaddr *, socklen_t *);
ssize_t net_sendto(struct File *, uintptr_t, size_t, int, const struct sockaddr *, socklen_t );
int     net_setsockopt(struct File *, int, int, const void *, socklen_t);
ssize_t net_read(struct File *, uintptr_t, size_t);
ssize_t net_write(struct File *, uintptr_t, size_t);
int     net_select(struct File *, struct timeval *);
int     net_gethostbyname(const char *, ip_addr_t *);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
