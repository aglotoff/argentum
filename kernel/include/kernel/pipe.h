#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/waitqueue.h>
#include <kernel/spinlock.h>

struct Pipe {
  struct KSpinLock lock;
  char              *buf;
  int                read_open;
  int                write_open;
  size_t             read_pos;
  size_t             write_pos;
  size_t             size;
  size_t             max_size;
  struct KWaitQueue read_queue;
  struct KWaitQueue write_queue;
};

void    pipe_init(void);
int     pipe_open(struct File **, struct File **);
int     pipe_close(struct File *);
ssize_t pipe_read(struct File *, uintptr_t, size_t);
ssize_t pipe_write(struct File *, uintptr_t, size_t);
int     pipe_stat(struct File *, struct stat *);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
