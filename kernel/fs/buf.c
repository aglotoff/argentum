#include <assert.h>

#include <cprintf.h>
#include <drivers/sd.h>
#include <list.h>
#include <mm/kmem.h>
#include <sync.h>

#include <fs/buf.h>

struct KMemCache *buf_desc_cache;

// Maximum size of the buffer cache
#define BUF_CACHE_MAX_SIZE   32

static struct {
  size_t          size;
  struct ListLink head;
  struct SpinLock lock;
} buf_cache;

static void
buf_ctor(void *ptr, size_t size)
{
  struct Buf *buf = (struct Buf *) ptr;

  buf->block_size = BLOCK_SIZE;
  list_init(&buf->wait_queue);
  mutex_init(&buf->mutex, "buf");

  (void) size;
}

/**
 * Initialize the buffer cache.
 */
void
buf_init(void)
{
  buf_desc_cache = kmem_cache_create("buf_desc_cache", sizeof(struct Buf), 0, buf_ctor, NULL);
  if (buf_desc_cache == NULL)
    panic("cannot allocate buf_desc_cache");

  spin_init(&buf_cache.lock, "buf_cache");
  list_init(&buf_cache.head);
}

static struct Buf *
buf_alloc(void)
{
  struct Buf *buf;

  assert(spin_holding(&buf_cache.lock));
  assert(buf_cache.size < BUF_CACHE_MAX_SIZE);

  if ((buf = (struct Buf *) kmem_alloc(buf_desc_cache)) == NULL)
    return NULL;

  buf->block_no   = 0;
  buf->dev        = 0;
  buf->flags      = 0;
  buf->ref_count  = 0;
  
  list_add_front(&buf_cache.head, &buf->cache_link);
  buf_cache.size++;

  return buf;
}

// Scan the cache for a buffer with the given block number and device.
static struct Buf *
buf_get(unsigned block_no, dev_t dev)
{
  struct ListLink *l;
  struct Buf *b, *unused = NULL;

  spin_lock(&buf_cache.lock);

  // TODO: use a hash table for faster lookups
  LIST_FOREACH(&buf_cache.head, l) {
    b = LIST_CONTAINER(l, struct Buf, cache_link);

    if ((b->block_no == block_no) && (b->dev == dev)) {
      b->ref_count++;

      spin_unlock(&buf_cache.lock);

      return b;
    }

    // Remember the least recently used free buffer.
    if (b->ref_count == 0)
      unused = b;
  }

  // Grow the buffer cache. If the maximum cache size is already reached, try
  // to reuse a buffer that held a different block.
  if ((buf_cache.size >= BUF_CACHE_MAX_SIZE) || ((b = buf_alloc()) == NULL))
    b = unused;

  if (b == NULL) {
    // Out of free blocks.
    spin_unlock(&buf_cache.lock);
    return NULL;
  }

  b->block_no  = block_no;
  b->dev       = dev;
  b->ref_count = 1;
  b->flags     = 0;

  spin_unlock(&buf_cache.lock);

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
buf_read(unsigned block_no, dev_t dev)
{
  struct Buf *buf;

  if ((buf = buf_get(block_no, dev)) == NULL)
    return NULL;

  mutex_lock(&buf->mutex);

  // If needed, read the block from the device.
  // TODO: check for I/O errors
  if (!(buf->flags & BUF_VALID))
    sd_request(buf);

  assert(buf->flags & BUF_VALID);

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
  if (!mutex_holding(&buf->mutex))
    panic("not holding buf->mutex");

  // TODO: check for I/O errors
  buf->flags |= BUF_DIRTY;
  sd_request(buf);
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
    warn("buffer isn't valid");
  
  if (buf->flags & BUF_DIRTY)
    warn("buffer is dirty");
  
  mutex_unlock(&buf->mutex);

  spin_lock(&buf_cache.lock);

  assert(buf->ref_count > 0);

  if (--buf->ref_count == 0) {
    // Return the buffer to the cache.
    list_remove(&buf->cache_link);
    list_add_front(&buf_cache.head, &buf->cache_link);
  }

  spin_unlock(&buf_cache.lock);
}
