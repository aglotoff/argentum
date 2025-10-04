#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>

struct Request;
struct IpcMessage;

void    pipe_init_system(void);
int     pipe_open(struct Connection **, struct Connection **);
void    pipe_close(struct Request *, struct IpcMessage *);
void    pipe_read(struct Request *, struct IpcMessage *);
void    pipe_write(struct Request *, struct IpcMessage *);
void    pipe_stat(struct Request *, struct IpcMessage *);
int     pipe_select(struct Connection *, struct timeval *);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
