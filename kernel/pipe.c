#include <errno.h>
#include <fcntl.h>

#include <kernel/cprintf.h>
#include <kernel/fs/file.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/pipe.h>
#include <kernel/waitqueue.h>

static struct KObjectPool *pipe_cache;

void
pipe_init(void)
{
  pipe_cache = k_object_pool_create("pipe", sizeof(struct Pipe), 0, NULL, NULL);
  if (pipe_cache == NULL)
    panic("cannot allocate pipe cache");
}

int
pipe_open(struct File **read_store, struct File **write_store)
{
  struct Pipe *pipe;
  struct Page *page;
  struct File *read, *write;
  int r;

  if ((pipe = (struct Pipe *) k_object_pool_get(pipe_cache)) == NULL) {
    r = -ENOMEM;
    goto fail1;
  }

  if ((page = page_alloc_one(0)) == NULL) {
    r = -ENOMEM;
    goto fail2;
  }

  pipe->data = (char *) page2kva(page);
  page->ref_count++;

  if ((r = file_alloc(&read)) < 0)
    goto fail3;

  if ((r = file_alloc(&write)) < 0)
    goto fail4;

  k_spinlock_init(&pipe->lock, "pipe");
  pipe->read_open  = 1;
  pipe->write_open = 1;
  pipe->read_pos   = 0;
  pipe->write_pos  = 0;
  pipe->size       = 0;
  k_waitqueue_init(&pipe->read_queue);
  k_waitqueue_init(&pipe->write_queue);

  read->type  = FD_PIPE;
  read->pipe  = pipe;
  read->flags = O_RDONLY;
  read->ref_count++;

  write->type  = FD_PIPE;
  write->pipe  = pipe;
  write->flags = O_WRONLY;
  write->ref_count++;

  *read_store = read;
  *write_store = write;

  return 0;

fail4:
  file_put(read);
fail3:
  page->ref_count--;
  page_free_one(page);
fail2:
  k_object_pool_put(pipe_cache, pipe);
fail1:
  return r;
}

int
pipe_close(struct File *file)
{
  struct Page *page;
  struct Pipe *pipe = file->pipe;
  int write = (file->flags & O_ACCMODE) != O_RDONLY;

  if (file->type != FD_PIPE)
    return -EBADF;

  k_spinlock_acquire(&pipe->lock);

  if (write) {
    pipe->write_open = 0;
    if (pipe->read_open) {
      k_waitqueue_wakeup_all(&pipe->read_queue);
    }
  } else {
    pipe->read_open = 0;
    if (pipe->write_open) {
      k_waitqueue_wakeup_all(&pipe->write_queue);
    }
  }

  if (pipe->read_open || pipe->write_open) {
    k_spinlock_release(&pipe->lock);
    return 0;
  }

  k_spinlock_release(&pipe->lock);

  page = kva2page(pipe->data);
  page->ref_count--;
  page_free_one(page);

  k_object_pool_put(pipe_cache, pipe);

  return 0;
}

ssize_t
pipe_read(struct File *file, void *buf, size_t n)
{
  char *dst = (char *) buf;
  size_t i;
  struct Pipe *pipe = file->pipe;

  if (file->type != FD_PIPE)
    return -EBADF;

  k_spinlock_acquire(&pipe->lock);

  while (pipe->write_open && (pipe->size == 0)) {
    int r;

    if ((r = k_waitqueue_sleep(&pipe->read_queue, &pipe->lock)) < 0) {
      k_spinlock_release(&pipe->lock);
      return r;
    }
  }

  for (i = 0; (i < n) && (pipe->size > 0); i++, pipe->size--) {
    dst[i] = pipe->data[pipe->read_pos++];
    
    if (pipe->read_pos == PAGE_SIZE)
      pipe->read_pos = 0;
  }

  k_waitqueue_wakeup_all(&pipe->write_queue);

  k_spinlock_release(&pipe->lock);

  return i;
}

ssize_t
pipe_write(struct File *file, const void *buf, size_t n)
{
  const char *src = (const char *) buf;
  size_t i;
  struct Pipe *pipe = file->pipe;

  if (file->type != FD_PIPE)
    return -EBADF;
  
  k_spinlock_acquire(&pipe->lock);

  for (i = 0; i < n; i++) {
    while (pipe->read_open && (pipe->size == PAGE_SIZE)) {
      int r;

      if ((r = k_waitqueue_sleep(&pipe->write_queue, &pipe->lock)) < 0) {
        k_spinlock_release(&pipe->lock);
        return r;
      }
    }

    pipe->data[pipe->write_pos++] = src[i];

    if (pipe->write_pos == PAGE_SIZE)
      pipe->write_pos = 0;

    if (pipe->size++ == 0)
      k_waitqueue_wakeup_all(&pipe->read_queue);
  }

  k_spinlock_release(&pipe->lock);

  return i;
}
