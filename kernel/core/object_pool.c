#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/types.h>
#include <kernel/object_pool.h>
#include <kernel/mm/page.h>

static int                    object_pool_setup(struct ObjectPool *,
                                                const char *, size_t, size_t,
                                                void (*)(void *, size_t),
                                                void (*)(void *, size_t));
static struct ObjectPoolSlab *object_pool_slab_create(struct ObjectPool *);
static void                   object_pool_slab_destroy(struct ObjectPoolSlab *);
static void                  *object_pool_slab_get(struct ObjectPoolSlab *);
static void                   object_pool_slab_put(struct ObjectPoolSlab *,
                                                   void *);                            

// Linked list of all object caches.
static struct {
  struct ListLink head;
  struct SpinLock lock;
} pool_list = {
  LIST_INITIALIZER(pool_list.head),
  SPIN_INITIALIZER("pool_list"),
};

// Pool of pool descriptors
static struct ObjectPool pool_of_pools;

static int
object_pool_setup(struct ObjectPool *cache,
                const char *name,
                size_t size,
                size_t align,
                void (*ctor)(void *, size_t),
                void (*dtor)(void *, size_t))
{
  size_t wastage, block_size;
  unsigned slab_page_order, slab_capacity;

  if (size < align)
    return -EINVAL;
  if ((PAGE_SIZE % align) != 0)
    return -EINVAL;

  align = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);

  block_size = ROUND_UP(size + sizeof(struct ObjectPoolTag), align);

  // Calculate the optimal slab capacity trying to 
  for (slab_page_order = 0; ; slab_page_order++) {
    size_t used = 0;
    size_t total = (PAGE_SIZE << slab_page_order);

    // TODO: off-slab data structures
    size_t extra = block_size + sizeof(struct ObjectPoolTag *);

    if (slab_page_order > PAGE_ORDER_MAX)
      return -EINVAL;

    // Determine max slab capacity
    // TODO: off-slab data structures
    used = sizeof(struct ObjectPoolSlab);
    for (slab_capacity = 1; used + extra <= total; slab_capacity++) {
      used += extra;
    }
    slab_capacity--;

    if (slab_capacity == 0)
      continue;

    // Limit internal fragmentation to 12.5% (1/8).
    wastage = total - used;
    if ((wastage * 8) <= total)
      break;
  }

  spin_init(&cache->lock, name);

  list_init(&cache->slabs_empty);
  list_init(&cache->slabs_partial);
  list_init(&cache->slabs_full);

  cache->slab_capacity   = slab_capacity;
  cache->slab_page_order = slab_page_order;
  cache->block_size      = block_size;
  cache->block_align     = align;
  cache->obj_size        = size;
  cache->obj_ctor        = ctor;
  cache->obj_dtor        = dtor;
  cache->color_max       = wastage;
  cache->color_next      = 0;

  spin_lock(&pool_list.lock);
  list_add_back(&pool_list.head, &cache->link);
  spin_unlock(&pool_list.lock);

  strncpy(cache->name, name, OBJECT_POOL_NAME_MAX);
  cache->name[OBJECT_POOL_NAME_MAX] = '\0';

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
struct ObjectPool *
object_pool_create(const char *name,
                   size_t size,
                   size_t align,
                   void (*ctor)(void *, size_t),
                   void (*dtor)(void *, size_t))
{
  struct ObjectPool *cache;
  
  if ((cache = object_pool_get(&pool_of_pools)) == NULL)
    return NULL;

  if (object_pool_setup(cache, name, size, align, ctor, dtor) != 0) {
    object_pool_put(&pool_of_pools, cache);
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
object_pool_destroy(struct ObjectPool *cache)
{
  struct ObjectPoolSlab *slab;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_empty) || !list_empty(&cache->slabs_partial)) {
    spin_unlock(&cache->lock);
    return -EBUSY;
  }

  while (!list_empty(&cache->slabs_full)) {
    slab = LIST_CONTAINER(cache->slabs_full.next, struct ObjectPoolSlab, link);
    list_remove(&slab->link);
    object_pool_slab_destroy(slab);
  }

  spin_unlock(&cache->lock);

  spin_lock(&pool_list.lock);
  list_remove(&cache->link);
  spin_unlock(&pool_list.lock);

  object_pool_put(&pool_of_pools, cache);

  return 0;
}

static struct ObjectPoolTag *
object_to_tag(struct ObjectPoolSlab *slab, void *obj)
{
  (void) slab;
  return (struct ObjectPoolTag *) obj;
}

static void *
tag_to_object(struct ObjectPoolSlab *slab, struct ObjectPoolTag *tag)
{
  (void) slab;
  return tag;
}

static struct ObjectPoolSlab *
object_pool_slab_create(struct ObjectPool *pool)
{
  struct ObjectPoolSlab *slab;
  struct Page *page;
  uint8_t *data, *object;
  unsigned i;

  assert(spin_holding(&pool->lock));

  if ((page = page_alloc_block(pool->slab_page_order, 0)) == NULL)
    return NULL;

  data = (uint8_t *) page2kva(page);
  page->ref_count++;

  slab = (struct ObjectPoolSlab *) &data[PAGE_SIZE << pool->slab_page_order] - 1;

  for (i = 0; i < (1U << pool->slab_page_order); i++)
    page[i].slab = slab;

  data += pool->color_next;

  // Calculate the next color offset.
  pool->color_next += pool->block_align;
  if (pool->color_next > pool->color_max)
    pool->color_next = 0;

  slab->data       = data;
  slab->pool       = pool;
  slab->used_count = 0;
  slab->free       = NULL;

  object = data;
  for (i = 0; i < pool->slab_capacity; i++) {
    struct ObjectPoolTag *tag = (struct ObjectPoolTag *) object;

    // TODO: off-slab structures
    tag->object = object;

    tag->next  = slab->free;
    slab->free = tag;

    if (pool->obj_ctor)
      pool->obj_ctor(object, pool->obj_size);

    object += pool->block_size + sizeof(struct ObjectPoolTag *);
    
    assert((void *) object <= (void *) slab);
  }

  cprintf("put to full %p\n", slab);
  list_add_back(&pool->slabs_full, &slab->link);

  return slab;
}

static void
object_pool_slab_destroy(struct ObjectPoolSlab *slab)
{
  struct ObjectPool *pool = slab->pool;
  struct Page *page;
  unsigned i;
  
  assert(spin_holding(&slab->pool->lock));
  assert(slab->used_count == 0);

  // Call destructor for all objects
  if (slab->pool->obj_dtor != NULL) {
    struct ObjectPoolTag *tag;

    for (tag = slab->free; tag != NULL; tag = tag->next)
      pool->obj_dtor(tag_to_object(slab, tag), pool->obj_size);
  }

  // Free the page containing the data
  // TODO: off-slab data structures
  page = kva2page(slab->data);

  for (i = 0; i < (1U << pool->slab_page_order); i++)
    page[i].slab = NULL;

  page->ref_count--;
  page_free_block(page, pool->slab_page_order);
}

static void *
object_pool_slab_get(struct ObjectPoolSlab *slab)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectPoolTag *tag;

  cprintf("get: %s %p %d - ", slab->pool->name, slab, pool->slab_capacity);

  // assert(slab->used_count < pool->slab_capacity);
  if (slab->used_count >= pool->slab_capacity) {
    panic("%s, %p, %d %d (%p %p)\n", slab->pool->name, slab, slab->used_count, pool->slab_capacity, 
    slab->link.prev, slab->link.next);
  }
  assert(slab->free != NULL);

  tag = slab->free;
  slab->free = tag->next;
  slab->used_count++;

  cprintf("%d of %d\n", slab->used_count, pool->slab_capacity);

  if (slab->used_count == pool->slab_capacity) {
    assert(slab->free == NULL);

    list_remove(&slab->link);
    list_add_back(&pool->slabs_empty, &slab->link);

    cprintf("Empty %p of %s: %p %p\n", slab, pool->name, slab->link.prev, slab->link.next);
  }

  return tag_to_object(slab, tag);
}

static void
object_pool_slab_put(struct ObjectPoolSlab *slab, void *obj)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectPoolTag *tag;
  
  assert(slab->used_count > 0);

  tag = object_to_tag(slab, obj);

  tag->next = slab->free;
  slab->free = tag;

  slab->used_count--;

  if (slab->used_count == 0) {
    list_remove(&slab->link);
     cprintf("put to full putput %p\n", slab);
    list_add_front(&pool->slabs_full, &slab->link);
  } else if (slab->used_count == pool->slab_capacity - 1) {
    list_remove(&slab->link);
    list_add_front(&pool->slabs_partial, &slab->link);
  }
}

void *
object_pool_get(struct ObjectPool *cache)
{
  struct ObjectPoolSlab *slab;
  void *obj;
  
  spin_lock(&cache->lock);

  if (!list_empty(&cache->slabs_partial)) {
    slab = LIST_CONTAINER(cache->slabs_partial.next, struct ObjectPoolSlab, link);
  } else {
    if (!list_empty(&cache->slabs_full)) {
      slab = LIST_CONTAINER(cache->slabs_full.next, struct ObjectPoolSlab, link);
      list_remove(&slab->link);
    } else if ((slab = object_pool_slab_create(cache)) == NULL) {
      spin_unlock(&cache->lock);
      return NULL;
    }

    list_add_back(&cache->slabs_partial, &slab->link);
  } 

  obj = object_pool_slab_get(slab);

  spin_unlock(&cache->lock);

  return obj;
}

void
object_pool_put(struct ObjectPool *cache, void *obj)
{
  struct Page *page;
  struct ObjectPoolSlab *slab;

  spin_lock(&cache->lock);

  page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << cache->slab_page_order));
  slab = page->slab;

  object_pool_slab_put(slab, obj);

  spin_unlock(&cache->lock);
}

void
object_pool_init(void)
{
  object_pool_setup(&pool_of_pools,
                  "pool_of_pools",
                  sizeof(struct ObjectPool),
                  0,
                  NULL,
                  NULL);
}
