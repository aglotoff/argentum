#include <assert.h>

#include "buf.h"
#include "kobject.h"
#include "list.h"
#include "sd.h"
#include "sync.h"

static struct KObjectPool *buf_pool;

static struct {
  unsigned        size;
  struct ListLink head;
  struct SpinLock lock;
} buf_cache;

void
buf_init(void)
{ 
  spin_init(&buf_cache.lock, "buf_cache");
  list_init(&buf_cache.head);

  buf_pool = kobject_pool_create("buf_pool", sizeof(struct Buf), 0);
  if (buf_pool == NULL)
    panic("cannot allocate buf_pool");
}

static struct Buf *
buf_alloc(void)
{
  struct Buf *buf;

  assert(spin_holding(&buf_cache.lock));
  assert(buf_cache.size < BUF_CACHE_SIZE);

  if ((buf = (struct Buf *) kobject_alloc(buf_pool)) == NULL)
    return NULL;

  buf->flags = 0;
  buf->ref_count = 0;
  buf->block_no = 0;
  mutex_init(&buf->mutex, "buf");
  list_init(&buf->wait_queue);

  list_add_front(&buf_cache.head, &buf->cache_link);
  buf_cache.size++;

  return buf;
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

      mutex_lock(&b->mutex);

      return b;
    }

    if (b->ref_count == 0 && !(b->flags & BUF_DIRTY))
      last_usable = b;
  }

  if (buf_cache.size < BUF_CACHE_SIZE) {
    if ((b = buf_alloc()) == NULL) {
      spin_unlock(&buf_cache.lock);
      return NULL;
    }
  } else {
    if ((b = last_usable) == NULL) {
      spin_unlock(&buf_cache.lock);
      return NULL;
    }
  }

  b->flags = 0;
  b->ref_count = 1;
  b->block_no = block_no;

  spin_unlock(&buf_cache.lock);

  mutex_lock(&b->mutex);

  return b;
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
  if (!mutex_holding(&buf->mutex))
    panic("not holding buf");
  
  mutex_unlock(&buf->mutex);

  spin_lock(&buf_cache.lock);

  assert(buf->ref_count > 0);

  if (--buf->ref_count == 0) {
    list_remove(&buf->cache_link);
    list_add_front(&buf_cache.head, &buf->cache_link);
  }

  spin_unlock(&buf_cache.lock);
}
