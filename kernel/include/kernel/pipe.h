#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/wchan.h>
#include <kernel/spinlock.h>

struct Pipe {
  struct SpinLock lock;
  char              *data;
  int                read_open;
  int                write_open;
  size_t             read_pos;
  size_t             write_pos;
  size_t             size;
  struct WaitChannel read_queue;
  struct WaitChannel write_queue;
};

void    pipe_init(void);
int     pipe_alloc(struct File **, struct File **);
void    pipe_close(struct Pipe *, int);
ssize_t pipe_read(struct Pipe *, void *, size_t);
ssize_t pipe_write(struct Pipe *, const void *, size_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
