#include <errno.h>
#include <fcntl.h>

#include <kernel/cprintf.h>
#include <kernel/fs/file.h>
#include <kernel/object_pool.h>
#include <kernel/mm/page.h>
#include <kernel/pipe.h>
#include <kernel/wchan.h>

static struct ObjectPool *pipe_cache;

void
pipe_init(void)
{
  pipe_cache = object_pool_create("pipe", sizeof(struct Pipe), 0, 0, NULL, NULL);
  if (pipe_cache == NULL)
    panic("cannot allocate pipe cache");
}

int
pipe_alloc(struct File **read_store, struct File **write_store)
{
  struct Pipe *pipe;
  struct Page *page;
  struct File *read, *write;
  int r;

  if ((pipe = (struct Pipe *) object_pool_get(pipe_cache)) == NULL) {
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

  spin_init(&pipe->lock, "pipe");
  pipe->read_open  = 1;
  pipe->write_open = 1;
  pipe->read_pos   = 0;
  pipe->write_pos  = 0;
  pipe->size       = 0;
  wchan_init(&pipe->read_queue);
  wchan_init(&pipe->write_queue);

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
  file_close(read);
fail3:
  page->ref_count--;
  page_free_one(page);
fail2:
  object_pool_put(pipe_cache, pipe);
fail1:
  return r;
}

void
pipe_close(struct Pipe *pipe, int write)
{
  struct Page *page;

  spin_lock(&pipe->lock);

  if (write) {
    pipe->write_open = 0;
    if (pipe->read_open) {
      wchan_wakeup_all(&pipe->read_queue);
    }
  } else {
    pipe->read_open = 0;
    if (pipe->write_open) {
      wchan_wakeup_all(&pipe->write_queue);
    }
  }

  if (pipe->read_open || pipe->write_open) {
    spin_unlock(&pipe->lock);
    return;
  }

  spin_unlock(&pipe->lock);

  page = kva2page(pipe->data);
  page->ref_count--;
  page_free_one(page);

  //object_pool_put(pipe_cache, pipe);
}

ssize_t
pipe_read(struct Pipe *pipe, void *buf, size_t n)
{
  char *dst = (char *) buf;
  size_t i;
  
  spin_lock(&pipe->lock);

  while (pipe->write_open && (pipe->size == 0)) {
    int r;

    if ((r = wchan_sleep(&pipe->read_queue, &pipe->lock)) < 0) {
      spin_unlock(&pipe->lock);
      return r;
    }
  }

  for (i = 0; (i < n) && (pipe->size > 0); i++, pipe->size--) {
    dst[i] = pipe->data[pipe->read_pos++];
    
    if (pipe->read_pos == PAGE_SIZE)
      pipe->read_pos = 0;
  }

  wchan_wakeup_all(&pipe->write_queue);

  spin_unlock(&pipe->lock);

  return i;
}

ssize_t
pipe_write(struct Pipe *pipe, const void *buf, size_t n)
{
  const char *src = (const char *) buf;
  size_t i;
  
  spin_lock(&pipe->lock);

  for (i = 0; i < n; i++) {
    while (pipe->read_open && (pipe->size == PAGE_SIZE)) {
      int r;

      if ((r = wchan_sleep(&pipe->write_queue, &pipe->lock)) < 0) {
        spin_unlock(&pipe->lock);
        return r;
      }
    }

    pipe->data[pipe->write_pos++] = src[i];

    if (pipe->write_pos == PAGE_SIZE)
      pipe->write_pos = 0;

    if (pipe->size++ == 0)
      wchan_wakeup_all(&pipe->read_queue);
  }

  spin_unlock(&pipe->lock);

  return i;
}
