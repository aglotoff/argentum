#include <assert.h>

#include "buf.h"
#include "list.h"
#include "sd.h"
#include "sleeplock.h"
#include "spinlock.h"

static struct {
  struct Buf buf[BUF_CACHE_SIZE];
  struct Spinlock lock;
  struct ListLink head;
} buf_cache;

void
buf_init(void)
{
  struct Buf *b;
  
  spin_init(&buf_cache.lock, "buf_cache");
  list_init(&buf_cache.head);

  for (b = buf_cache.buf; b < &buf_cache.buf[BUF_CACHE_SIZE]; b++) {
    sleep_init(&b->lock, "buf");
    list_init(&b->wait_queue.head);

    list_add_back(&buf_cache.head, &b->cache_link);
  }
}

static struct Buf *
buf_get(unsigned block_no)
{
  struct ListLink *l;
  struct Buf *b, *last_usable;

  spin_lock(&buf_cache.lock);

  last_usable = NULL;
  LIST_FOREACH(&buf_cache.head, l) {
    b = LIST_CONTAINER(l, struct Buf, cache_link);
    if (b->block_no == block_no) {
      b->ref_count++;

      spin_unlock(&buf_cache.lock);

      sleep_lock(&b->lock);

      return b;
    }

    if (b->ref_count == 0 && !(b->flags & BUF_DIRTY))
      last_usable = b;
  }

  if (last_usable != NULL) {
    last_usable = last_usable;
    last_usable->flags = 0;
    last_usable->ref_count = 1;
    last_usable->block_no = block_no;

    spin_unlock(&buf_cache.lock);

    sleep_lock(&last_usable->lock);

    return last_usable;
  }

  spin_unlock(&buf_cache.lock);

  return NULL;
}

struct Buf *
buf_read(unsigned block_no)
{
  struct Buf *buf;

  if ((buf = buf_get(block_no)) == NULL)
    panic("cannot get block %d", block_no);

  if (!(buf->flags & BUF_VALID))
    sd_request(buf);

  return buf;
}

void
buf_write(struct Buf *buf)
{
  (void) buf;
}

void 
buf_release(struct Buf *buf)
{
  if (!sleep_holding(&buf->lock))
    panic("not holding buf");
  
  sleep_unlock(&buf->lock);

  spin_lock(&buf_cache.lock);

  assert(buf->ref_count > 0);

  if (--buf->ref_count == 0) {
    list_remove(&buf->cache_link);
    list_add_front(&buf_cache.head, &buf->cache_link);
  }

  spin_unlock(&buf_cache.lock);
}
