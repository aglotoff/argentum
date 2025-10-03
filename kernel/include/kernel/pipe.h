#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>

struct Request;
struct IpcMessage;

void    pipe_init_system(void);
int     pipe_open(struct Connection **, struct Connection **);
int     pipe_close(struct Connection *);
void pipe_read(struct Request *, struct IpcMessage *);
ssize_t pipe_write(struct Connection *, uintptr_t, size_t);
int     pipe_stat(struct Connection *, uintptr_t);
int     pipe_select(struct Connection *, struct timeval *);

intptr_t pipe_send_recv(struct Connection *, void *, size_t, void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
