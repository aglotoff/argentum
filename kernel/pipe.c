#include <errno.h>
#include <fcntl.h>

#include <kernel/console.h>
#include <kernel/fs/file.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/pipe.h>
#include <kernel/waitqueue.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>

static struct KObjectPool *pipe_cache;

static const size_t PIPE_BUF_ORDER = 1;
static const size_t PIPE_BUF_SIZE  = (PAGE_SIZE << PIPE_BUF_ORDER);

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

  if ((page = page_alloc_block(PIPE_BUF_ORDER, 0, PAGE_TAG_PIPE)) == NULL) {
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
  page_free_block(page, PIPE_BUF_ORDER);
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
  page_free_block(page, PIPE_BUF_ORDER);

  k_object_pool_put(pipe_cache, pipe);

  return 0;
}

ssize_t
pipe_read(struct File *file, uintptr_t va, size_t n)
{
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
  
  // TODO: could copy all available data in one or two steps

  for (i = 0; (i < n) && (pipe->size > 0); i++, pipe->size--) {
    int r;

    r = vm_space_copy_out(&pipe->data[pipe->read_pos++], va + i, 1);

    if (pipe->read_pos == PIPE_BUF_SIZE)
      pipe->read_pos = 0;

    if (r < 0) {
      k_waitqueue_wakeup_all(&pipe->write_queue);
      k_spinlock_release(&pipe->lock);
      return r;
    }
  }

  k_waitqueue_wakeup_all(&pipe->write_queue);

  k_spinlock_release(&pipe->lock);

  return i;
}

ssize_t
pipe_write(struct File *file, uintptr_t va, size_t n)
{
  size_t i;
  struct Pipe *pipe = file->pipe;

  if (file->type != FD_PIPE)
    return -EBADF;

  k_spinlock_acquire(&pipe->lock);

  // TODO: could copy all available data in one or two steps

  for (i = 0; i < n; i++) {
    int r;

    while (pipe->read_open && (pipe->size == PIPE_BUF_SIZE)) {
      if ((r = k_waitqueue_sleep(&pipe->write_queue, &pipe->lock)) < 0) {
        k_spinlock_release(&pipe->lock);
        return r;
      }
    }

    r = vm_space_copy_in(&pipe->data[pipe->write_pos++], va + i, 1);
  
    if (pipe->write_pos == PIPE_BUF_SIZE)
      pipe->write_pos = 0;

    if (pipe->size++ == 0)
      k_waitqueue_wakeup_all(&pipe->read_queue);

    if (r < 0) {
      k_spinlock_release(&pipe->lock);
      return r;
    }
  }

  k_spinlock_release(&pipe->lock);

  return i;
}

int
pipe_stat(struct File *file, struct stat *buf)
{
  struct Pipe *pipe = file->pipe;

  if (file->type != FD_PIPE)
    return -EBADF;
  
  k_spinlock_acquire(&pipe->lock);

  // TODO: use meaningful values
  buf->st_dev          = 255;
  buf->st_ino          = 0;
  buf->st_mode         = _IFIFO;
  buf->st_nlink        = 0;
  buf->st_uid          = 0;
  buf->st_gid          = 0;
  buf->st_rdev         = 0;
  buf->st_size         = 0;
  buf->st_atim.tv_sec  = 0;
  buf->st_atim.tv_nsec = 0;
  buf->st_mtim.tv_sec  = 0;
  buf->st_mtim.tv_nsec = 0;
  buf->st_ctim.tv_sec  = 0;
  buf->st_ctim.tv_nsec = 0;
  buf->st_blocks       = 0;
  buf->st_blksize      = 0;

  k_spinlock_release(&pipe->lock);

  return 0;
}
