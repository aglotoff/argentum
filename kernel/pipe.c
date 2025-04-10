#include <errno.h>
#include <fcntl.h>

#include <kernel/console.h>
#include <kernel/ipc/channel.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/time.h>

static struct KObjectPool *pipe_cache;

static const size_t PIPE_BUF_ORDER = 4;
static const size_t PIPE_BUF_SIZE  = (PAGE_SIZE << PIPE_BUF_ORDER);

void
pipe_init(void)
{
  pipe_cache = k_object_pool_create("pipe", sizeof(struct Pipe), 0, NULL, NULL);
  if (pipe_cache == NULL)
    k_panic("cannot allocate pipe cache");
}

int
pipe_open(struct Channel **read_store, struct Channel **write_store)
{
  struct Pipe *pipe;
  struct Page *page;
  struct Channel *read, *write;
  int r;

  if ((pipe = (struct Pipe *) k_object_pool_get(pipe_cache)) == NULL) {
    r = -ENOMEM;
    goto fail1;
  }

  if ((page = page_alloc_block(PIPE_BUF_ORDER, 0, PAGE_TAG_PIPE)) == NULL) {
    r = -ENOMEM;
    goto fail2;
  }

  pipe->buf = (char *) page2kva(page);
  page->ref_count++;

  if ((r = channel_alloc(&read)) < 0)
    goto fail3;

  if ((r = channel_alloc(&write)) < 0)
    goto fail4;

  k_mutex_init(&pipe->mutex, "pipe");
  pipe->read_open  = 1;
  pipe->write_open = 1;
  pipe->read_pos   = 0;
  pipe->write_pos  = 0;
  pipe->size       = 0;
  pipe->max_size   = PIPE_BUF_SIZE;
  k_condvar_create(&pipe->read_cond);
  k_condvar_create(&pipe->write_cond);

  read->type  = CHANNEL_TYPE_PIPE;
  read->u.pipe = pipe;
  read->flags = O_RDONLY;
  read->ref_count++;

  write->type  = CHANNEL_TYPE_PIPE;
  write->u.pipe = pipe;
  write->flags = O_WRONLY;
  write->ref_count++;

  *read_store = read;
  *write_store = write;

  return 0;

fail4:
  channel_unref(read);
fail3:
  page_assert(page, PIPE_BUF_ORDER, PAGE_TAG_PIPE);
  page->ref_count--;
  page_free_block(page, PIPE_BUF_ORDER);
fail2:
  k_object_pool_put(pipe_cache, pipe);
fail1:
  return r;
}

int
pipe_close(struct Channel *file)
{
  struct Page *page;
  struct Pipe *pipe = file->u.pipe;
  int write = (file->flags & O_ACCMODE) != O_RDONLY;
  int r;

  if (file->type != CHANNEL_TYPE_PIPE)
    return -EBADF;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  if (write) {
    pipe->write_open = 0;
    if (pipe->read_open) {
      k_condvar_broadcast(&pipe->read_cond);
    }
  } else {
    pipe->read_open = 0;
    if (pipe->write_open) {
      k_condvar_broadcast(&pipe->write_cond);
    }
  }

  if (pipe->read_open || pipe->write_open) {
    k_mutex_unlock(&pipe->mutex);
    return 0;
  }

  k_mutex_unlock(&pipe->mutex);

  page = kva2page(pipe->buf);
  page_assert(page, PIPE_BUF_ORDER, PAGE_TAG_PIPE);

  page->ref_count--;
  page_free_block(page, PIPE_BUF_ORDER);

  k_mutex_fini(&pipe->mutex);
  k_condvar_destroy(&pipe->read_cond);
  k_condvar_destroy(&pipe->write_cond);

  k_object_pool_put(pipe_cache, pipe);

  return 0;
}

int
pipe_select(struct Channel *file, struct timeval *timeout)
{
  struct Pipe *pipe = file->u.pipe;
  int r;

  if (file->type != CHANNEL_TYPE_PIPE)
    return -EBADF;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  while (pipe->size == 0) {
    unsigned long t = 0;

    if (timeout != NULL) {
      t = timeval2ticks(timeout);
      k_mutex_unlock(&pipe->mutex);
      return 0;
    }

    if ((r = k_condvar_timed_wait(&pipe->read_cond, &pipe->mutex, t)) < 0) {
      k_mutex_unlock(&pipe->mutex);
      return r;
    }
  }

  k_mutex_unlock(&pipe->mutex);

  return 1;
}

ssize_t
pipe_read(struct Channel *file, uintptr_t va, size_t n)
{
  size_t i;
  struct Pipe *pipe = file->u.pipe;
  int r;

  if (file->type != CHANNEL_TYPE_PIPE)
    return -EBADF;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  page_assert(kva2page(pipe->buf), PIPE_BUF_ORDER, PAGE_TAG_PIPE);

  while (pipe->write_open && (pipe->size == 0)) {
    int r;

    if ((r = k_condvar_wait(&pipe->read_cond, &pipe->mutex)) < 0) {
      k_mutex_unlock(&pipe->mutex);
      return r;
    }
  }
  
  // TODO: could copy all available data in one or two steps

  for (i = 0; (i < n) && (pipe->size > 0); ) {
    size_t nread;
    int r;

    nread = MIN(pipe->size, n - i);
    if (pipe->read_pos + nread > pipe->max_size)
      // Prevent overflow
      nread = pipe->max_size - pipe->read_pos;

    r = vm_space_copy_out(thread_current(), &pipe->buf[pipe->read_pos], va + i, nread);
    if (r < 0) {
      k_condvar_broadcast(&pipe->write_cond);
      k_mutex_unlock(&pipe->mutex);

      return r;
    }

    pipe->read_pos += nread;
    i += nread;
    pipe->size -= nread;

    if (pipe->read_pos == pipe->max_size)
      pipe->read_pos = 0;
  }

  k_condvar_broadcast(&pipe->write_cond);
  k_mutex_unlock(&pipe->mutex);

  return i;
}

ssize_t
pipe_write(struct Channel *file, uintptr_t va, size_t n)
{
  size_t i;
  struct Pipe *pipe = file->u.pipe;
  int r;

  if (file->type != CHANNEL_TYPE_PIPE)
    return -EBADF;

  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

  page_assert(kva2page(pipe->buf), PIPE_BUF_ORDER, PAGE_TAG_PIPE);

  for (i = 0; i < n; ) {
    size_t nwrite;
    int r;

    while (pipe->read_open && (pipe->size == pipe->max_size)) {
      if ((r = k_condvar_wait(&pipe->write_cond, &pipe->mutex)) < 0) {
        k_mutex_unlock(&pipe->mutex);
        return r;
      }
    }

    nwrite = MIN(pipe->max_size - pipe->size, n - i);
    if (pipe->write_pos + nwrite > pipe->max_size)
      // Prevent overflow
      nwrite = pipe->max_size - pipe->write_pos;

    r = vm_space_copy_in(thread_current(), &pipe->buf[pipe->write_pos], va + i, nwrite);
    if (r < 0) {
      k_mutex_unlock(&pipe->mutex);
      return r;
    }

    if (pipe->size == 0)
      k_condvar_broadcast(&pipe->read_cond);

    i += nwrite;
    pipe->size += nwrite;
    pipe->write_pos += nwrite;

    if (pipe->write_pos == pipe->max_size)
      pipe->write_pos = 0;
  }

  k_mutex_unlock(&pipe->mutex);

  return i;
}

int
pipe_stat(struct Channel *file, struct stat *buf)
{
  struct Pipe *pipe = file->u.pipe;
  int r;

  if (file->type != CHANNEL_TYPE_PIPE)
    return -EBADF;
  
  if ((r = k_mutex_lock(&pipe->mutex)) < 0)
    return r;

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

  k_mutex_unlock(&pipe->mutex);

  return 0;
}
