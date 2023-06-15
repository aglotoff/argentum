#ifndef __KERNEL_INCLUDE_KERNEL_NET_H__
#define __KERNEL_INCLUDE_KERNEL_NET_H__

#include <stddef.h>

void net_enqueue(void *, size_t);
void net_init(void);

#endif  // !__KERNEL_INCLUDE_KERNEL_NET_H__
