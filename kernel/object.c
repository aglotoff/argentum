
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/kernel.h>
#include <kernel/object.h>
#include <kernel/page.h>

// Linked list of all object caches
static struct {
  struct ListLink head;
  struct SpinLock lock;
} cache_list = {
  LIST_INITIALIZER(cache_list.head),
  SPIN_INITIALIZER("cache_list"),
};

// Cache for cache descriptors
static struct ObjectCache cache_cache;

static int                object_cache_init(struct ObjectCache *, const char *,
                                            size_t, size_t,
                                            void (*)(void *, size_t),
                                            void (*)(void *, size_t));
static struct ObjectSlab *object_slab_create(struct ObjectCache *);
static void               object_slab_destroy(struct ObjectCache *,
                                              struct ObjectSlab *);
static void               *object_alloc_one(struct ObjectCache *,
                                            struct ObjectSlab *);
static void                object_free_one(struct ObjectCache *,
                                           struct ObjectSlab *, void *);

/**
 * @brief Create object cache.
 * 
 * Creates a cache for objects, each size obj_size, aligned on a align
 * boundary.
 * 
 * @param name     Identifies the cache for statistics and debugging.
 * @param size     The size of each object in bytes.
 * @param align    The alignment of each object (or 0 if no special alignment)
 *                 is required).
 *
 * @return Pointer to the cache descriptor or NULL if out of memory.
 */
struct ObjectCache *
object_cache_create(const char *name, size_t obj_size, size_t align,
                    void (*ctor)(void *, size_t), void (*dtor)(void *, size_t))
{
  struct ObjectCache *cache;
  
  if ((cache = (struct ObjectCache *) object_alloc(&cache_cache)) == NULL)
    return NULL;

  if (object_cache_init(cache, name, obj_size, align, ctor, dtor) != 0) {
    object_free(&cache_cache, cache);
    return NULL;
  }

  return cache;
}

/**
 * @brief Destory object cache.
 * 
 * Destroys the cache and reclaims all associated resources.
 * 
 * @param cache The cache descriptor to be destroyed.
 */
int
object_cache_destroy(struct ObjectCache *cache)
{
  struct ObjectSlab *slab;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_empty) || !list_empty(&cache->slabs_partial)) {
    spin_unlock(&cache->lock);
    return -EBUSY;
  }

  while (!list_empty(&cache->slabs_full)) {
    slab = LIST_CONTAINER(cache->slabs_full.next, struct ObjectSlab, link);
    list_remove(&slab->link);
    object_slab_destroy(cache, slab);
  }

  spin_unlock(&cache->lock);

  spin_lock(&cache_list.lock);
  list_remove(&cache->link);
  spin_unlock(&cache_list.lock);

  object_free(&cache_cache, cache);

  return 0;
}

void *
object_alloc(struct ObjectCache *cache)
{
  struct ObjectSlab *slab;
  void *obj;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_partial)) {
    slab = LIST_CONTAINER(cache->slabs_partial.next, struct ObjectSlab, link);
  } else {
    if (!list_empty(&cache->slabs_full)) {
      slab = LIST_CONTAINER(cache->slabs_full.next, struct ObjectSlab, link);
      list_remove(&slab->link);
    } else if ((slab = object_slab_create(cache)) == NULL) {
      spin_unlock(&cache->lock);
      return NULL;
    }

    list_add_back(&cache->slabs_partial, &slab->link);
  } 

  obj = object_alloc_one(cache, slab);

  spin_unlock(&cache->lock);

  return obj;
}

void
object_free(struct ObjectCache *cache, void *obj)
{
  struct Page *page;
  struct ObjectSlab *slab;

  spin_lock(&cache->lock);

  page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << cache->slab_page_order));
  slab = page->slab;

  object_free_one(cache, slab, obj);

  spin_unlock(&cache->lock);
}

void
object_init(void)
{
  object_cache_init(&cache_cache,
                    "cache_cache",
                    sizeof(struct ObjectCache),
                    0,
                    NULL,
                    NULL);
}

static int
object_cache_init(struct ObjectCache *cache,
                  const char *name,
                  size_t obj_size,
                  size_t align,
                  void (*ctor)(void *, size_t),
                  void (*dtor)(void *, size_t))
{
  size_t wastage, buf_size;
  unsigned slab_page_order, slab_capacity;

  if (obj_size < align)
    return -EINVAL;
  if ((PAGE_SIZE % align) != 0)
    return -EINVAL;

  align = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);

  buf_size = ROUND_UP(obj_size + sizeof(struct ObjectBufCtl), align);

  // Calculate the optimal slab capacity trying to limit internal fragmentation
  // to 12.5% (1/8).
  // TODO: off-slab data structures
  for (slab_page_order = 0; ; slab_page_order++) {
    if (slab_page_order > PAGE_ORDER_MAX)
      return -EINVAL;

    wastage = (PAGE_SIZE << slab_page_order) - sizeof(struct ObjectSlab);
    slab_capacity = wastage / buf_size;
    wastage -= slab_capacity * buf_size;

    if ((wastage * 8) <= (PAGE_SIZE << slab_page_order))
      break;
  }

  spin_init(&cache->lock, name);

  list_init(&cache->slabs_empty);
  list_init(&cache->slabs_partial);
  list_init(&cache->slabs_full);

  cache->slab_capacity   = slab_capacity;
  cache->slab_page_order = slab_page_order;
  cache->buf_size        = buf_size;
  cache->buf_align       = align;
  cache->obj_size        = obj_size;
  cache->ctor            = ctor;
  cache->dtor            = dtor;
  cache->color_max       = wastage;
  cache->color_next      = 0;

  spin_lock(&cache_list.lock);
  list_add_back(&cache_list.head, &cache->link);
  spin_unlock(&cache_list.lock);

  strncpy(cache->name, name, OBJECT_CACHE_NAME_MAX);
  cache->name[OBJECT_CACHE_NAME_MAX] = '\0';

  return 0;
}

static void *
bufctl_to_object(struct ObjectCache *cache, struct ObjectBufCtl *bufctl)
{
  // TODO: off-slab data structures
  return (uint8_t *) (bufctl + 1) - cache->buf_size;
}

static void *
object_to_bufctl(struct ObjectCache *cache, void *obj)
{
  // TODO: off-slab data structures
  return (struct ObjectBufCtl *) ((uint8_t *) obj + cache->buf_size) - 1;
}

static void
object_slab_init_all(struct ObjectCache *cache, struct ObjectSlab *slab)
{
  struct ObjectBufCtl *bufctl, **prev_bufctl;
  uint8_t *p;
  unsigned i;

  assert(spin_holding(&cache->lock));

  prev_bufctl = &slab->free;
  for (i = 0, p = slab->buf;
       i < cache->slab_capacity;
       i++, p += cache->buf_size) {
    // Place the bufctl structure at the end of the buffer.
    bufctl = object_to_bufctl(cache, p);

    bufctl->next = NULL;
    *prev_bufctl = bufctl;
    prev_bufctl  = &bufctl->next;

    if (cache->ctor != NULL)
      cache->ctor(p, cache->obj_size);
  }
}

static struct ObjectSlab *
object_slab_create(struct ObjectCache *cache)
{
  struct ObjectSlab *slab;
  struct Page *page;
  uint8_t *buf;

  assert(spin_holding(&cache->lock));

  if ((page = page_alloc_block(cache->slab_page_order, 0)) == NULL)
    return NULL;

  buf = (uint8_t *) page2kva(page);
  page->ref_count++;

  // Place slab data at the end of the page block.
  // TODO: off-slab data structures
  slab = (struct ObjectSlab *) (buf + (PAGE_SIZE << cache->slab_page_order)) - 1;
  
  slab->buf    = buf + cache->color_next;
  slab->in_use = 0;

  object_slab_init_all(cache, slab);

  page->slab = slab;

  // Calculate the next color offset.
  cache->color_next += cache->buf_align;
  if (cache->color_next > cache->color_max)
    cache->color_next = 0;

  return slab;
}

static void
object_slab_destroy_all(struct ObjectCache *cache, struct ObjectSlab *slab)
{
  struct ObjectBufCtl *bufctl;

  if (cache->dtor == NULL)
    return;
  
  for (bufctl = slab->free; bufctl != NULL; bufctl = bufctl->next)
    cache->dtor(bufctl_to_object(cache, bufctl), cache->obj_size);
}

static void
object_slab_destroy(struct ObjectCache *cache, struct ObjectSlab *slab)
{
  struct Page *page;
  
  assert(spin_holding(&cache->lock));
  assert(slab->in_use == 0);

  object_slab_destroy_all(cache, slab);

  // TODO: off-slab data structures
  page = kva2page(slab->buf);
  page->ref_count--;
  page_free_block(page, cache->slab_page_order);
}

static void *
object_alloc_one(struct ObjectCache *cache, struct ObjectSlab *slab)
{
  struct ObjectBufCtl *bufctl;

  assert(slab->in_use < cache->slab_capacity);
  assert(slab->free != NULL);

  bufctl     = slab->free;
  slab->free = bufctl->next;
  slab->in_use++;

  if (slab->in_use == cache->slab_capacity) {
    assert(slab->free == NULL);

    list_remove(&slab->link);
    list_add_back(&cache->slabs_empty, &slab->link);
  }

  return bufctl_to_object(cache, bufctl);
}

static void
object_free_one(struct ObjectCache *cache, struct ObjectSlab *slab, void *obj)
{
  struct ObjectBufCtl *bufctl;
  
  assert(slab->in_use < cache->slab_capacity);

  bufctl = object_to_bufctl(cache, obj);

  bufctl->next = slab->free;
  slab->free   = bufctl;
  slab->in_use--;

  if (slab->in_use == 0) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_full, &slab->link);
  } else if (slab->in_use == cache->slab_capacity - 1) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_partial, &slab->link);
  }
}
