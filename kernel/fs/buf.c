#include <kernel/core/assert.h>
#include <kernel/dev.h>
#include <kernel/console.h>
#include <kernel/fs/buf.h>
#include <kernel/core/list.h>
#include <kernel/object_pool.h>
#include <kernel/core/spinlock.h>
#include <kernel/page.h>
#include <kernel/hash.h>

struct KObjectPool *buf_pool;

static void buf_request(struct Buf *, int);

#define BUF_CACHE_MAX_SIZE    256

#define NBUCKET               64

enum {
  BUF_FLAGS_VALID = (1 << 0),
  BUF_FLAGS_DIRTY = (1 << 1),
  BUF_FLAGS_ERROR = (1 << 2),
};

static struct {
  struct KListLink hash_table[NBUCKET];
  size_t           size;
  struct KListLink head;
  struct KSpinLock lock;
} buf_cache;

static uint32_t
buf_hash(unsigned block_no, dev_t dev)
{
  uint32_t x = (uint32_t) block_no;
  uint32_t y = (uint32_t) dev;

  // Simple 32-bit mix (inspired by boost/hash_combine)
  uint32_t h = x;
  h ^= y + 0x9e3779b9u + (h << 6) + (h >> 2);

  return h;
}

static void
buf_ctor(void *ptr, size_t)
{
  struct Buf *buf = (struct Buf *) ptr;
  
  k_mutex_init(&buf->_mutex, "buf");
}

// no buf_dtor since buffers stay in cache forever (for now)

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

  HASH_INIT(buf_cache.hash_table);
}

static uint8_t *
buf_alloc_page_data(size_t block_size)
{
  unsigned page_order;
  struct Page *page;

  page_order = page_estimate_order(block_size);

  if ((page = page_alloc_block(page_order, 0, PAGE_TAG_BUF)) == NULL)
    return NULL;

  page_inc_ref(page);

  return (uint8_t *) page2kva(page);
}

static uint8_t *
buf_alloc_data(size_t block_size)
{
  if (block_size < PAGE_SIZE)
    return (uint8_t *) k_malloc(block_size);
  return buf_alloc_page_data(block_size);
}

static void
buf_free_page_data(void *data, size_t block_size)
{
  unsigned page_order;
  struct Page *page;

  page_order = page_estimate_order(block_size);

  page = kva2page(data);

#ifndef NDEBUG
  page_assert(page, page_order, PAGE_TAG_BUF);
#endif

  page_dec_ref(page, page_order);
}

static void
buf_free_data(void *data, size_t block_size)
{
  if (block_size < PAGE_SIZE)
    k_free(data);
  else
    buf_free_page_data(data, block_size);
}

static void
buf_cache_insert(struct Buf *buf)
{
  if (!k_list_is_null(&buf->_cache_link))
    k_list_remove(&buf->_cache_link);

  k_list_add_back(&buf_cache.head, &buf->_cache_link);
}

static struct Buf *
buf_alloc(unsigned block_no, size_t block_size, dev_t dev)
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

  buf->_ref_count = 0;
  buf->block_no   = block_no;
  buf->dev        = dev;
  buf->_flags     = 0;
  buf->block_size = block_size;
  k_list_null(&buf->_cache_link);
  k_list_null(&buf->_hash_link);

  return buf;
}

static struct Buf *
buf_cache_grow(unsigned block_no, size_t block_size, dev_t dev)
{
  struct Buf *buf;

  if (buf_cache.size >= BUF_CACHE_MAX_SIZE)
    return NULL;

  if ((buf = buf_alloc(block_no, block_size, dev)) == NULL)
    return NULL;

  HASH_PUT(buf_cache.hash_table, &buf->_hash_link, buf_hash(block_no, dev));

  buf_cache_insert(buf);
  buf_cache.size++;

  return buf;
}

static struct Buf *
buf_cache_lookup(unsigned block_no, dev_t dev, struct Buf **unused_store)
{
  struct KListLink *l;

  unsigned hash = buf_hash(block_no, dev);
  HASH_FOREACH_ENTRY(buf_cache.hash_table, l, hash) {
    struct Buf *buf;
    
    buf = K_CONTAINER_OF(l, struct Buf, _hash_link);
    if ((buf->block_no == block_no) && (buf->dev == dev))
      return buf;
  }

  if (unused_store) {
    if (k_list_is_empty(&buf_cache.head))
      *unused_store = NULL;
    else
      *unused_store = K_CONTAINER_OF(buf_cache.head.next, struct Buf, _cache_link);
  }

  return NULL;
}

static struct Buf *
buf_reuse(struct Buf *b, unsigned block_no, size_t block_size, dev_t dev)
{
  if (b->block_size != block_size) {
    uint8_t *data;

    if ((data = buf_alloc_data(block_size)) == NULL)
      return NULL;

    buf_free_data(b->data, b->block_size);

    b->data = data;
    b->block_size = block_size;
  }

  k_list_remove(&b->_hash_link);

  HASH_PUT(buf_cache.hash_table, &b->_hash_link, buf_hash(block_no, dev));

  b->block_no   = block_no;
  b->dev        = dev;
  b->_flags      = 0;

  return b;
}

static struct Buf *
buf_cache_get_locked(unsigned block_no, size_t block_size, dev_t dev)
{
  struct Buf *b, *unused = NULL;

  if ((b = buf_cache_lookup(block_no, dev, &unused)) != NULL) {
    if (b->block_size != block_size)
      return buf_reuse(b, block_no, block_size, dev);
    return b;
  }

  if ((b = buf_cache_grow(block_no, block_size, dev)) != NULL)
    return b;

  if (unused == NULL)
    return NULL;

  return buf_reuse(unused, block_no, block_size, dev);
}

static struct Buf *
buf_cache_get(unsigned block_no, size_t block_size, dev_t dev)
{
  struct Buf *b;

  k_spinlock_acquire(&buf_cache.lock);

  if ((b = buf_cache_get_locked(block_no, block_size, dev)) != NULL) {
    k_list_remove(&b->_cache_link);
    b->_ref_count++;
  }

  k_spinlock_release(&buf_cache.lock);

  return b;
}

#ifndef NDEBUG
static void
buf_assert(struct Buf *buf)
{
  unsigned page_order; 
  struct Page *page;

  if (buf->block_size < PAGE_SIZE)
    return;

  page_order = page_estimate_order(buf->block_size);
  page = kva2page(buf->data);

  page_assert(page, page_order, PAGE_TAG_BUF);
}
#endif

struct Buf *
buf_read(unsigned block_no, size_t block_size, dev_t dev)
{
  struct Buf *buf;
  int r;

  k_assert(block_no != (unsigned) -1);

  if ((buf = buf_cache_get(block_no, block_size, dev)) == NULL)
    return NULL;

  r = k_mutex_lock(&buf->_mutex);
  k_assert(r == 0);

#ifndef NDEBUG
  buf_assert(buf);
#endif

  // If needed, read the block contents.
  if (!(buf->_flags & BUF_FLAGS_VALID))
    buf_request(buf, BUF_REQUEST_READ);
  
  // TODO: check error
  buf->_flags |= BUF_FLAGS_VALID;

  return buf;
}

void
buf_write(struct Buf *buf)
{
  buf->_flags |= BUF_FLAGS_DIRTY;
  buf_release(buf);
}

static void
buf_cache_put(struct Buf *buf)
{
  k_spinlock_acquire(&buf_cache.lock);

  if (--buf->_ref_count == 0)
    buf_cache_insert(buf);

  k_spinlock_release(&buf_cache.lock);
}

void 
buf_release(struct Buf *buf)
{ 
  k_assert(buf->_flags & BUF_FLAGS_VALID);

  if (buf->_flags & BUF_FLAGS_DIRTY) {
    k_assert(k_mutex_holding(&buf->_mutex));

#ifndef NDEBUG
    buf_assert(buf);
#endif

    buf_request(buf, BUF_REQUEST_WRITE);

    // TODO: check for I/O errors
    buf->_flags &= ~BUF_FLAGS_DIRTY;
  }

  k_mutex_unlock(&buf->_mutex);

  buf_cache_put(buf);
}

static void
buf_request_init(struct BufRequest *req, struct Buf *buf, int type)
{
  req->buf = buf;
  req->type = type;
  k_list_null(&req->queue_link);
  k_condvar_create(&req->_wait_cond);
}

static void
buf_request(struct Buf *buf, int type)
{
  struct BlockDev *dev;
  struct BufRequest req;

  k_assert(k_mutex_holding(&buf->_mutex));
  k_assert((buf->_flags & (BUF_FLAGS_DIRTY | BUF_FLAGS_VALID)) != BUF_FLAGS_VALID);

  if ((dev = dev_lookup_block(buf->dev)) == NULL)
    k_panic("no block device %d found", buf->dev);

  buf_request_init(&req, buf, type);

  dev->request(&req);
}
