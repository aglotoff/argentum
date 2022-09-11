#include <assert.h>
#include <errno.h>
#include <string.h>

#include <cprintf.h>
#include <types.h>
#include <mm/page.h>
#include <sync.h>

#include <mm/kmem.h>

// static int           kmem_cache_grow(struct KMemCache *);
static unsigned         kmem_cache_estimate(size_t, unsigned, int, size_t *);
static struct KMemSlab *kmem_slab_alloc(struct KMemCache *);
static void             kmem_slab_destroy(struct KMemCache *,
                                          struct KMemSlab *);

// Linked list of all object caches.
static struct {
  struct ListLink head;
  struct SpinLock lock;
} cache_list = {
  LIST_INITIALIZER(cache_list.head),
  SPIN_INITIALIZER("cache_list"),
};

// cache for cache descriptors.
static struct KMemCache cache_cache = {
  .slabs_empty   = LIST_INITIALIZER(cache_cache.slabs_empty),
  .slabs_partial = LIST_INITIALIZER(cache_cache.slabs_partial),
  .slabs_full    = LIST_INITIALIZER(cache_cache.slabs_full),
  .lock          = SPIN_INITIALIZER("cache_cache"),
  .obj_size      = sizeof(struct KMemCache),
  .obj_align     = sizeof(uintptr_t),
  .name          = "cache_cache",
};

// Cache for off-slab slab descriptors.
static struct KMemCache slab_cache = {
  .slabs_empty   = LIST_INITIALIZER(slab_cache.slabs_empty),
  .slabs_partial = LIST_INITIALIZER(slab_cache.slabs_partial),
  .slabs_full    = LIST_INITIALIZER(slab_cache.slabs_full),
  .lock          = SPIN_INITIALIZER("slab_cache"),
  .obj_size      = sizeof(struct KMemSlab),
  .obj_align     = sizeof(uintptr_t),
  .name          = "slab_cache",
};

/**
 * @brief Create object cache.
 * 
 * Creates a cache for objects, each size obj_size, aligned on a align
 * boundary.
 * 
 * @param name     Identifies the cache for statistics and debugging.
 * @param obj_size The size of each object in bytes.
 * @param align    The alignment of each object (or 0 if no special alignment)
 *                 is required).
 *
 * @return Pointer to the cache descriptor or NULL if out of memory.
 */
struct KMemCache *
kmem_cache_create(const char *name, size_t obj_size, size_t align,
                  void (*ctor)(void *, size_t), void (*dtor)(void *, size_t))
{
  struct KMemCache *cache;
  size_t wastage;
  unsigned page_order, slab_capacity;
  int flags;

  if ((cache = kmem_alloc(&cache_cache)) == NULL)
    return NULL;

  // Force word size alignment
  align    = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);
  obj_size = ROUND_UP(obj_size, align);

  // For objects larger that 1/8 of a page, keep descriptors off slab.
  // flags = (obj_size >= (PAGE_SIZE / 8)) ? KMEM_CACHE_OFFSLAB : 0;
  flags = 0;

  // Estimate slab size
  for (page_order = 0; ; page_order++) {
    if (page_order > PAGE_ORDER_MAX) {
      kmem_free(&cache_cache, cache);
      return NULL;
    }

    // Try to limit internal fragmentation to 12.5% (1/8).
    slab_capacity = kmem_cache_estimate(obj_size, page_order, flags, &wastage);
    if ((wastage * 8) <= (PAGE_SIZE << page_order))
      break;
  }

  list_init(&cache->slabs_empty);
  list_init(&cache->slabs_partial);
  list_init(&cache->slabs_full);

  cache->flags           = flags;
  cache->obj_size        = obj_size;
  cache->slab_capacity   = slab_capacity;
  cache->slab_page_order = page_order;
  cache->obj_ctor        = ctor;
  cache->obj_dtor        = dtor;
  cache->obj_align       = align;
  cache->color_max       = wastage;
  cache->color_next      = 0;

  spin_init(&cache->lock, name);
  cache->name         = name;

  spin_lock(&cache_list.lock);
  list_add_back(&cache_list.head, &cache->link);
  spin_unlock(&cache_list.lock);

  return cache;
}

//
// Calculate the number of objects that would fit into a slab of size
// 2^page_order pages and how much space would be left over.
//
static unsigned
kmem_cache_estimate(size_t obj_size,
                      unsigned page_order,
                      int flags,
                      size_t *left_over)
{
  size_t wastage;
  unsigned slab_capacity;

  wastage = (PAGE_SIZE << page_order);
  if (!(flags & KMEM_CACHE_OFFSLAB))
    wastage -= sizeof(struct KMemSlab);

  slab_capacity = wastage / (obj_size + sizeof(struct KMemBufCtl));

  if (left_over) {
    wastage -= slab_capacity * (obj_size + sizeof(struct KMemBufCtl));
    *left_over = wastage;
  }

  return slab_capacity;
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

  while(!list_empty(&cache->slabs_full)) {
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

/*
 * ----------------------------------------------------------------------------
 * Slab management
 * ----------------------------------------------------------------------------
 */

static struct KMemSlab *
kmem_slab_alloc(struct KMemCache *cache)
{
  struct KMemSlab *slab;
  struct Page *page;
  struct KMemBufCtl *curr, **prevp;
  unsigned num;
  uint8_t *buf, *p;

  assert(spin_holding(&cache->lock));

  // Allocate page block for the slab.
  if ((page = page_alloc_block(cache->slab_page_order, 0)) == NULL)
    return NULL;

  buf = (uint8_t *) page2kva(page);

  if (cache->flags & KMEM_CACHE_OFFSLAB) {
    // If slab descriptors are kept off pages, allocate one from the slab cache.
    if ((slab = (struct KMemSlab *) kmem_alloc(&slab_cache)) == NULL) {
      page_free_block(page, cache->slab_page_order);
      return NULL;
    }
  } else {
    // Otherwise, store the slab descriptor at the end of the page frame.
    slab = (struct KMemSlab *) (buf + (PAGE_SIZE << cache->slab_page_order)) - 1;
  }

  page->ref_count++;
  page->slab = slab;

  slab->in_use = 0;
  slab->buf = buf;

  // Initialize all objects.
  p = buf + cache->color_next;
  prevp = &slab->free;
  for (num = 0; num < cache->slab_capacity; num++) {
    if (cache->obj_ctor != NULL)
      cache->obj_ctor((struct KMemBufCtl *) p, cache->obj_size);

    curr = (struct KMemBufCtl *) (p + cache->obj_size);
    curr->next = NULL;

    *prevp = curr;
    prevp = &curr->next;

    p = (uint8_t *) (curr + 1);
  }

  // Calculate color offset for the next slab.
  cache->color_next += cache->obj_align;
  if (cache->color_next > cache->color_max)
    cache->color_next = 0;

  return slab;
}

static void
kmem_slab_destroy(struct KMemCache *cache, struct KMemSlab *slab)
{
  struct Page *page;
  struct KMemBufCtl *p;
  
  assert(spin_holding(&cache->lock));
  assert(slab->in_use == 0);

  if (cache->obj_dtor != NULL) {
    for (p = slab->free; p != NULL; p = p->next) {
      cache->obj_dtor((uint8_t *) p - cache->obj_size, cache->obj_size);
    }
  }

  // Free the pages been used for the slab.
  page = kva2page(slab->buf);
  page->ref_count--;
  page_free_block(page, cache->slab_page_order);

  // If slab descriptor was kept off page, return it to the slab cache.
  if (cache->flags & KMEM_CACHE_OFFSLAB)
    kmem_free(&slab_cache, slab);
}

/*
 * ----------------------------------------------------------------------------
 * Object allocation
 * ----------------------------------------------------------------------------
 */

void *
kmem_alloc(struct KMemCache *cache)
{
  struct ListLink *list;
  struct KMemSlab *slab;
  struct KMemBufCtl *obj;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_partial)) {
    list = &cache->slabs_partial;
  } else {
    list = &cache->slabs_full;

    if (list_empty(list)) {
      if ((slab = kmem_slab_alloc(cache)) == NULL) {
        spin_unlock(&cache->lock);
        return NULL;
      }

      list_add_back(list, &slab->link);
    }
  }

  slab = LIST_CONTAINER(list->next, struct KMemSlab, link);

  assert(slab->in_use < cache->slab_capacity);
  assert(slab->free != NULL);

  if (++slab->in_use == cache->slab_capacity) {
    list_remove(&slab->link);
    list_add_back(&cache->slabs_empty, &slab->link);
  }

  obj = slab->free;
  slab->free = obj->next;

  spin_unlock(&cache->lock);

  return (uint8_t *) obj - cache->obj_size;
}

/*
 * ----------------------------------------------------------------------------
 * Object freeing
 * ----------------------------------------------------------------------------
 */

void
kmem_free(struct KMemCache *cache, void *obj)
{
  struct Page *slab_page;
  struct KMemSlab *slab;
  struct KMemBufCtl *node;

  slab_page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << cache->slab_page_order));
  slab = slab_page->slab;

  spin_lock(&cache->lock);

  node = (struct KMemBufCtl *) ((uint8_t *) obj + cache->obj_size);
  node->next = slab->free;
  slab->free = node;

  if (--slab->in_use > 0) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_partial, &slab->link);
  } else if (slab->in_use == 0) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_full, &slab->link);
  }

  spin_unlock(&cache->lock);
}

/*
 * ----------------------------------------------------------------------------
 * Initializing the object allocator
 * ----------------------------------------------------------------------------
 */

void
kmem_init(void)
{
  cache_cache.slab_capacity = kmem_cache_estimate(cache_cache.obj_size,
                                            cache_cache.slab_page_order, 
                                            cache_cache.flags,
                                            &cache_cache.color_max);
  list_add_back(&cache_list.head, &cache_cache.link);

  slab_cache.slab_capacity = kmem_cache_estimate(slab_cache.obj_size,
                                            slab_cache.slab_page_order, 
                                            slab_cache.flags,
                                            &slab_cache.color_max);
  list_add_back(&cache_list.head, &slab_cache.link);
}

