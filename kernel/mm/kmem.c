#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/types.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/page.h>

static int              kmem_cache_init(struct KMemCache *,
                                        const char *,
                                        size_t,
                                        size_t,
                                        void (*)(void *, size_t),
                                        void (*)(void *, size_t));

static struct KMemSlab *kmem_slab_create(struct KMemCache *);
static void             kmem_slab_destroy(struct KMemCache *,
                                          struct KMemSlab *);

static void            *kmem_bufctl_to_object(struct KMemCache *,
                                              struct KMemBufCtl *);
static void            *kmem_object_to_bufctl(struct KMemCache *, void *);

static void             kmem_slab_init_objects(struct KMemCache *,
                                               struct KMemSlab *);
static void             kmem_slab_destroy_objects(struct KMemCache *,
                                                  struct KMemSlab *);

static void            *kmem_alloc_one(struct KMemCache *,
                                       struct KMemSlab *);
static void             kmem_free_one(struct KMemCache *,
                                      struct KMemSlab *,
                                      void *);                            

// Linked list of all object caches.
static struct {
  struct ListLink head;
  struct SpinLock lock;
} cache_list = {
  LIST_INITIALIZER(cache_list.head),
  SPIN_INITIALIZER("cache_list"),
};

// cache for cache descriptors.
static struct KMemCache cache_cache;

static int
kmem_cache_init(struct KMemCache *cache,
                const char *name,
                size_t size,
                size_t align,
                void (*ctor)(void *, size_t),
                void (*dtor)(void *, size_t))
{
  size_t wastage, buf_size;
  unsigned slab_page_order, slab_capacity;

  if (size < align)
    return -EINVAL;
  if ((PAGE_SIZE % align) != 0)
    return -EINVAL;

  align = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);

  buf_size = ROUND_UP(size + sizeof(struct KMemBufCtl), align);

  // Calculate the optimal slab capacity trying to limit internal fragmentation
  // to 12.5% (1/8).
  // TODO: off-slab data structures
  for (slab_page_order = 0; ; slab_page_order++) {
    if (slab_page_order > PAGE_ORDER_MAX)
      return -EINVAL;

    wastage = (PAGE_SIZE << slab_page_order) - sizeof(struct KMemSlab);
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
  cache->obj_size        = size;
  cache->obj_ctor        = ctor;
  cache->obj_dtor        = dtor;
  cache->color_max       = wastage;
  cache->color_next      = 0;

  spin_lock(&cache_list.lock);
  list_add_back(&cache_list.head, &cache->link);
  spin_unlock(&cache_list.lock);

  strncpy(cache->name, name, KMEM_CACHE_NAME_MAX);
  cache->name[KMEM_CACHE_NAME_MAX] = '\0';

  return 0;
}

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
struct KMemCache *
kmem_cache_create(const char *name,
                  size_t size,
                  size_t align,
                  void (*ctor)(void *, size_t),
                  void (*dtor)(void *, size_t))
{
  struct KMemCache *cache;
  
  if ((cache = kmem_alloc(&cache_cache)) == NULL)
    return NULL;

  if (kmem_cache_init(cache, name, size, align, ctor, dtor) != 0) {
    kmem_free(&cache_cache, cache);
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
kmem_cache_destroy(struct KMemCache *cache)
{
  struct KMemSlab *slab;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_empty) || !list_empty(&cache->slabs_partial)) {
    spin_unlock(&cache->lock);
    return -EBUSY;
  }

  while (!list_empty(&cache->slabs_full)) {
    slab = LIST_CONTAINER(cache->slabs_full.next, struct KMemSlab, link);
    list_remove(&slab->link);
    kmem_slab_destroy(cache, slab);
  }

  spin_unlock(&cache->lock);

  spin_lock(&cache_list.lock);
  list_remove(&cache->link);
  spin_unlock(&cache_list.lock);

  kmem_free(&cache_cache, cache);

  return 0;
}

static void *
kmem_bufctl_to_object(struct KMemCache *cache, struct KMemBufCtl *bufctl)
{
  // TODO: off-slab data structures
  return (uint8_t *) (bufctl + 1) - cache->buf_size;
}

static void *
kmem_object_to_bufctl(struct KMemCache *cache, void *obj)
{
  // TODO: off-slab data structures
  return (struct KMemBufCtl *) ((uint8_t *) obj + cache->buf_size) - 1;
}

static void
kmem_slab_init_objects(struct KMemCache *cache, struct KMemSlab *slab)
{
  struct KMemBufCtl *bufctl, **prev_bufctl;
  uint8_t *p;
  unsigned i;

  assert(spin_holding(&cache->lock));

  prev_bufctl = &slab->free;
  for (i = 0, p = slab->buf;
       i < cache->slab_capacity;
       i++, p += cache->buf_size) {
    // Place the bufctl structure at the end of the buffer.
    bufctl = kmem_object_to_bufctl(cache, p);

    bufctl->next = NULL;
    *prev_bufctl = bufctl;
    prev_bufctl  = &bufctl->next;

    if (cache->obj_ctor != NULL)
      cache->obj_ctor(p, cache->obj_size);
  }
}

static void
kmem_slab_destroy_objects(struct KMemCache *cache, struct KMemSlab *slab)
{
  struct KMemBufCtl *bufctl;

  if (cache->obj_dtor == NULL)
    return;
  
  for (bufctl = slab->free; bufctl != NULL; bufctl = bufctl->next)
    cache->obj_dtor(kmem_bufctl_to_object(cache, bufctl), cache->obj_size);
}

static struct KMemSlab *
kmem_slab_create(struct KMemCache *cache)
{
  struct KMemSlab *slab;
  struct Page *page;
  uint8_t *buf;

  assert(spin_holding(&cache->lock));

  if ((page = page_alloc_block(cache->slab_page_order, 0)) == NULL)
    return NULL;

  buf = (uint8_t *) page2kva(page);
  page->ref_count++;

  // Place slab data at the end of the page block.
  // TODO: off-slab data structures
  slab = (struct KMemSlab *) (buf + (PAGE_SIZE << cache->slab_page_order)) - 1;
  
  slab->buf    = buf + cache->color_next;
  slab->in_use = 0;

  kmem_slab_init_objects(cache, slab);

  page->slab = slab;

  // Calculate the next color offset.
  cache->color_next += cache->buf_align;
  if (cache->color_next > cache->color_max)
    cache->color_next = 0;

  return slab;
}

static void
kmem_slab_destroy(struct KMemCache *cache, struct KMemSlab *slab)
{
  struct Page *page;
  
  assert(spin_holding(&cache->lock));
  assert(slab->in_use == 0);

  kmem_slab_destroy_objects(cache, slab);

  // TODO: off-slab data structures
  page = kva2page(slab->buf);
  page->ref_count--;
  page_free_block(page, cache->slab_page_order);
}

static void *
kmem_alloc_one(struct KMemCache *cache, struct KMemSlab *slab)
{
  struct KMemBufCtl *bufctl;

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

  return kmem_bufctl_to_object(cache, bufctl);
}

static void
kmem_free_one(struct KMemCache *cache, struct KMemSlab *slab, void *obj)
{
  struct KMemBufCtl *bufctl;
  
  assert(slab->in_use > 0);

  bufctl = kmem_object_to_bufctl(cache, obj);

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

void *
kmem_alloc(struct KMemCache *cache)
{
  struct KMemSlab *slab;
  void *obj;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_partial)) {
    slab = LIST_CONTAINER(cache->slabs_partial.next, struct KMemSlab, link);
  } else {
    if (!list_empty(&cache->slabs_full)) {
      slab = LIST_CONTAINER(cache->slabs_full.next, struct KMemSlab, link);
      list_remove(&slab->link);
    } else if ((slab = kmem_slab_create(cache)) == NULL) {
      spin_unlock(&cache->lock);
      return NULL;
    }

    list_add_back(&cache->slabs_partial, &slab->link);
  } 

  obj = kmem_alloc_one(cache, slab);

  spin_unlock(&cache->lock);

  return obj;
}

void
kmem_free(struct KMemCache *cache, void *obj)
{
  struct Page *page;
  struct KMemSlab *slab;

  spin_lock(&cache->lock);

  page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << cache->slab_page_order));
  slab = page->slab;

  kmem_free_one(cache, slab, obj);

  spin_unlock(&cache->lock);
}

void
kmem_init(void)
{
  kmem_cache_init(&cache_cache,
                  "cache_cache",
                  sizeof(struct KMemCache),
                  0,
                  NULL,
                  NULL);
}
