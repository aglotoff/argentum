#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>

void    pipe_init_system(void);
int     pipe_open(struct Channel **, struct Channel **);
int     pipe_close(struct Channel *);
ssize_t pipe_read(struct Channel *, uintptr_t, size_t);
ssize_t pipe_write(struct Channel *, uintptr_t, size_t);
int     pipe_stat(struct Channel *, uintptr_t);
int     pipe_select(struct Channel *, struct timeval *);

intptr_t pipe_send_recv(struct Channel *, void *, size_t, void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
