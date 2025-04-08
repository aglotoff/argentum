#ifndef __KERNEL_INCLUDE_KERNEL_PIPE_H__
#define __KERNEL_INCLUDE_KERNEL_PIPE_H__

#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>

struct Pipe {
  struct KMutex   mutex;
  char           *buf;
  int             read_open;
  int             write_open;
  size_t          read_pos;
  size_t          write_pos;
  size_t          size;
  size_t          max_size;
  struct KCondVar read_cond;
  struct KCondVar write_cond;
};

void    pipe_init(void);
int     pipe_open(struct Channel **, struct Channel **);
int     pipe_close(struct Channel *);
ssize_t pipe_read(struct Channel *, uintptr_t, size_t);
ssize_t pipe_write(struct Channel *, uintptr_t, size_t);
int     pipe_stat(struct Channel *, struct stat *);
int     pipe_select(struct Channel *, struct timeval *);

#endif  // !__KERNEL_INCLUDE_KERNEL_PIPE_H__
