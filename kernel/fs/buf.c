#include <kernel/core/assert.h>
#include <kernel/dev.h>
#include <kernel/console.h>
#include <kernel/fs/buf.h>
#include <kernel/core/list.h>
#include <kernel/object_pool.h>
#include <kernel/core/spinlock.h>
#include <kernel/page.h>

struct KObjectPool *buf_pool;

static void buf_request(struct Buf *);

// Maximum size of the buffer cache
#define BUF_CACHE_MAX_SIZE   1024

static struct {
  size_t          size;
  struct KListLink head;
  struct KSpinLock lock;
} buf_cache;

static void
buf_ctor(void *ptr, size_t)
{
  struct Buf *buf = (struct Buf *) ptr;

  k_condvar_create(&buf->wait_cond);
  k_mutex_init(&buf->mutex, "buf");
}

/**
 * Initialize the buffer cache.
 */
void
buf_init(void)
{
  buf_pool = k_object_pool_create("buf_pool",
                                  sizeof(struct Buf),
                                  0,
                                  buf_ctor,
                                  NULL);
  if (buf_pool == NULL)
    k_panic("cannot allocate buf_pool");

  k_spinlock_init(&buf_cache.lock, "buf_cache");
  k_list_init(&buf_cache.head);
}

static uint8_t *
buf_alloc_data(size_t block_size)
{
  unsigned long page_order;
  struct Page *page;

  if (block_size < PAGE_SIZE)
    return (uint8_t *) k_malloc(block_size);

  for (page_order = 0; (PAGE_SIZE << page_order) < block_size; page_order++)
    ;

  if ((page = page_alloc_block(page_order, 0, PAGE_TAG_BUF)) == NULL)
    return NULL;

  page->ref_count++;
  return (uint8_t *) page2kva(page);
}

static void
buf_free_data(void *data, size_t block_size)
{
  unsigned long page_order;
  struct Page *page;

  if (block_size < PAGE_SIZE) {
    k_free(data);
    return;
  }

  for (page_order = 0; (PAGE_SIZE << page_order) < block_size; page_order++)
    ;

  page = kva2page(data);
  page_assert(page, page_order, PAGE_TAG_BUF);

  page->ref_count--;
  page_free_block(page, page_order);
}

static struct Buf *
buf_alloc(size_t block_size)
{
  struct Buf *buf;

  k_assert(k_spinlock_holding(&buf_cache.lock));
  k_assert(buf_cache.size < BUF_CACHE_MAX_SIZE);

  if ((buf = (struct Buf *) k_object_pool_get(buf_pool)) == NULL)
    return NULL;

  if ((buf->data = buf_alloc_data(block_size)) == NULL) {
    k_object_pool_put(buf_pool, buf);
    return NULL;
  }

  buf->block_no   = 0;
  buf->dev        = 0;
  buf->flags      = 0;
  buf->ref_count  = 0;
  buf->block_size = block_size;
  buf->cache_link.prev = NULL;
  buf->cache_link.next = NULL;
  buf->queue_link.prev = NULL;
  buf->queue_link.next = NULL;

  k_list_add_front(&buf_cache.head, &buf->cache_link);
  buf_cache.size++;

  return buf;
}

// Scan the cache for a buffer with the given block number and device.
static struct Buf *
buf_get(unsigned block_no, size_t block_size, dev_t dev)
{
  struct KListLink *l;
  struct Buf *b, *unused = NULL;

  k_spinlock_acquire(&buf_cache.lock);

  // TODO: use a hash table for faster lookups
  KLIST_FOREACH(&buf_cache.head, l) {
    b = KLIST_CONTAINER(l, struct Buf, cache_link);

    if ((b->block_no == block_no) &&
        (b->dev == dev) &&
        (b->block_size == block_size)) {
      b->ref_count++;

      k_spinlock_release(&buf_cache.lock);

      return b;
    }

    // Remember the least recently used free buffer.
    if (b->ref_count == 0)
      unused = b;
  }

  // Grow the buffer cache. If the maximum cache size is already reached, try
  // to reuse a buffer that held a different block.
  if ((buf_cache.size >= BUF_CACHE_MAX_SIZE) ||
      ((b = buf_alloc(block_size)) == NULL))
    b = unused;

  if (b == NULL) {
    // Out of free blocks.
    k_spinlock_release(&buf_cache.lock);
    return NULL;
  }

  // TODO: realloc
  if (b->block_size != block_size) {
    buf_free_data(b->data, b->block_size);

    if ((b->data = buf_alloc_data(block_size)) == NULL) {
      k_list_remove(&b->cache_link);
      k_object_pool_put(buf_pool, b);

      k_spinlock_release(&buf_cache.lock);

      return NULL;
    }

    b->block_size = block_size;
  }

  b->block_size = block_size;
  b->block_no   = block_no;
  b->dev        = dev;
  b->ref_count  = 1;
  b->flags      = 0;

  k_spinlock_release(&buf_cache.lock);

  return b;
}

/**
 * Get a buffer for the given filesystem block from the cache.
 * 
 * @param block_no The filesystem block number.
 * @param dev      ID of the device the block belongs to.
 * @return A pointer to the locked Buf structure, or NULL if unable to allocate
 *         a block.
 */
struct Buf *
buf_read(unsigned block_no, size_t block_size, dev_t dev)
{
  struct Buf *buf;
  int r;

  if (block_no == (unsigned) -1)
    k_panic("bad block_no");

  if ((buf = buf_get(block_no, block_size, dev)) == NULL)
    return NULL;

  if ((r = k_mutex_lock(&buf->mutex)) < 0) {
    k_panic("TODO %d", r);
    return NULL;
  }

  if (block_size >= PAGE_SIZE) {
    unsigned page_order; 
    struct Page *page;

    for (page_order = 0; (PAGE_SIZE << page_order) < block_size; page_order++)
      ;

    page = kva2page(buf->data);
    page_assert(page, page_order, PAGE_TAG_BUF);
  }

  // If needed, read the block contents.
  // TODO: check for I/O errors
  if (!(buf->flags & BUF_VALID))
    buf_request(buf);

  k_assert(buf->flags & BUF_VALID);

  return buf;
}

/**
 * Write the buffer data to the disk. This function must be called before
 * releasing the buffer, if its data has changed. The caller must hold
 * 'buf->mutex'.
 * 
 * @param buf Pointer to the Buf structure to be written.
 */
void
buf_write(struct Buf *buf)
{
  if (!k_mutex_holding(&buf->mutex))
    k_panic("not holding buf->mutex");

  if (buf->block_size >= PAGE_SIZE) {
    unsigned page_order; 
    struct Page *page;

    for (page_order = 0; (PAGE_SIZE << page_order) < buf->block_size; page_order++)
      ;

    page = kva2page(buf->data);
    page_assert(page, page_order, PAGE_TAG_BUF);
  }

  // TODO: check for I/O errors
  buf_request(buf);
}

/**
 * Release the buffer.
 * 
 * @param buf Pointer to the Buf structure to be released.
 */
void 
buf_release(struct Buf *buf)
{ 
  if (!(buf->flags & BUF_VALID))
    k_panic("buffer not valid");

  if (buf->flags & BUF_DIRTY) {
    buf_write(buf);

    if (buf->flags & BUF_DIRTY) {
      k_panic("buffer still dirty");
    }
  }
  
  k_mutex_unlock(&buf->mutex);

  k_spinlock_acquire(&buf_cache.lock);

  if (--buf->ref_count == 0) {
    // Return the buffer to the cache.
    k_list_remove(&buf->cache_link);
    k_list_add_front(&buf_cache.head, &buf->cache_link);
  }

  k_spinlock_release(&buf_cache.lock);
}

/**
 * Add buffer to the request queue and put the current process to sleep until
 * the operation is completed.
 * 
 * @param buf The buffer to be processed.
 */
static void
buf_request(struct Buf *buf)
{
  struct BlockDev *dev;

  if (!k_mutex_holding(&buf->mutex))
    k_panic("buf not locked");
  if ((buf->flags & (BUF_DIRTY | BUF_VALID)) == BUF_VALID)
    k_panic("nothing to do");

  if ((dev = dev_lookup_block(buf->dev)) == NULL)
    k_panic("no block device %d found", buf->dev);

  dev->request(buf);
}
