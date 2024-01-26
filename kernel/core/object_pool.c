#include <kernel/assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/types.h>
#include <kernel/object_pool.h>
#include <kernel/mm/page.h>

static int                object_pool_init(struct ObjectPool *, const char *,
                                           size_t, size_t, int,
                                           void (*)(void *, size_t),
                                           void (*)(void *, size_t));
static struct ObjectSlab *object_pool_slab_create(struct ObjectPool *);
static void               object_pool_slab_destroy(struct ObjectSlab *);
static void              *object_pool_slab_get(struct ObjectSlab *);
static void               object_pool_slab_put(struct ObjectSlab *, void *);                            

// Linked list of all object pools.
static struct {
  struct ListLink head;
  struct SpinLock lock;
} pool_list = {
  LIST_INITIALIZER(pool_list.head),
  SPIN_INITIALIZER("pool_list"),
};

// Pool of pool descriptors
static struct ObjectPool pool_of_pools;

// Pool of slab descriptors
static struct ObjectPool *slab_pool;

#define ANON_POOLS_MIN_SIZE   8U
#define ANON_POOLS_MAX_SIZE   2048U
#define ANON_POOLS_LENGTH     9

static struct ObjectPool *anon_pools[ANON_POOLS_LENGTH];

/**
 * Create an object pool.
 * 
 * @param name  Identifies the pool for statistics and debugging
 * @param size  The size of each object in bytes
 * @param align The alignment of each object (or 0 if no special alignment is
 *              required).
 * @param ctor  Function to construct objects in the pool (or NULL)
 * @param dtor  Function to undo object construction in the pool (or NULL)
 *
 * @return Pointer to the pool descriptor or NULL if out of memory.
 */
struct ObjectPool *
object_pool_create(const char *name,
                   size_t size,
                   size_t align,
                   int flags,
                   void (*ctor)(void *, size_t),
                   void (*dtor)(void *, size_t))
{
  struct ObjectPool *pool;
  
  if ((pool = object_pool_get(&pool_of_pools)) == NULL)
    return NULL;

  if (object_pool_init(pool, name, size, align, flags, ctor, dtor) < 0) {
    object_pool_put(&pool_of_pools, pool);
    return NULL;
  }

  return pool;
}

/**
 * Destroy the pool and reclaim all associated resources.
 * 
 * @param pool The pool descriptor to be destroyed.
 */
int
object_pool_destroy(struct ObjectPool *pool)
{
  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_empty) || !list_empty(&pool->slabs_partial)) {
    spin_unlock(&pool->lock);
    return -EBUSY;
  }

  while (!list_empty(&pool->slabs_full)) {
    struct ObjectSlab *slab;

    slab = LIST_CONTAINER(pool->slabs_full.next, struct ObjectSlab, link);
    list_remove(&slab->link);

    object_pool_slab_destroy(slab);
  }

  spin_unlock(&pool->lock);

  spin_lock(&pool_list.lock);
  list_remove(&pool->link);
  spin_unlock(&pool_list.lock);

  object_pool_put(&pool_of_pools, pool);

  return 0;
}

void *
object_pool_get(struct ObjectPool *pool)
{
  struct ObjectSlab *slab;
  void *obj;
  
  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_partial)) {
    slab = LIST_CONTAINER(pool->slabs_partial.next, struct ObjectSlab, link);
  } else {
    if (!list_empty(&pool->slabs_full)) {
      slab = LIST_CONTAINER(pool->slabs_full.next, struct ObjectSlab, link);
    } else if ((slab = object_pool_slab_create(pool)) == NULL) {
      spin_unlock(&pool->lock);
      return NULL;
    }

    list_remove(&slab->link);
    list_add_back(&pool->slabs_partial, &slab->link);
  } 

  obj = object_pool_slab_get(slab);

  spin_unlock(&pool->lock);

  return obj;
}

void
object_pool_put(struct ObjectPool *pool, void *obj)
{
  struct Page *page;
  struct ObjectSlab *slab;

  spin_lock(&pool->lock);

  page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << pool->slab_page_order));
  slab = page->slab;

  object_pool_slab_put(slab, obj);

  spin_unlock(&pool->lock);
}

void
system_object_pool_init(void)
{ 
  int i;

  // First, solve the "chicken and egg" problem by initializing the static
  // pool of pool descriptors
  if (object_pool_init(&pool_of_pools, "pool_of_pools",
                       sizeof(struct ObjectPool), 0, 0, NULL, NULL) < 0)
    panic("cannot initialize pool_of_pools");

  // Then initialize the pool of slab descriptors (stored off-slab)
  slab_pool = object_pool_create("slab", sizeof(struct ObjectSlab),
                                 0, 0, NULL, NULL);
  if (slab_pool == NULL)
    panic("cannot create slab_pool");

  // Finally, initialize the anonymous pools (also used for arrays of off-slab
  // object tags)
  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t size = ANON_POOLS_MIN_SIZE << i;
    char name[64];

    snprintf(name, sizeof(name), "anon(%u)", size);

    anon_pools[i] = object_pool_create(name, size, 0, 0, NULL, NULL);
    if (anon_pools[i] == NULL)
      panic("cannot initialize %s", name);
  }
}

void *
kmalloc(size_t size)
{
  int i;

  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t pool_size = ANON_POOLS_MIN_SIZE << i;

    if (size <= pool_size)
      return object_pool_get(anon_pools[i]);
  }

  return NULL;
}

void
kfree(void *ptr)
{
  struct Page *page;

  // Determine the slab (and the pool) this pointer belongs to
  page = kva2page(ptr);
  if (page->slab == NULL)
    panic("bad pointer");

  object_pool_put(page->slab->pool, ptr);
}

static int
object_pool_init(struct ObjectPool *cache,
                 const char *name,
                 size_t size,
                 size_t align,
                 int flags,
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

  if (size > (PAGE_SIZE / 8))
    flags |= OBJECT_POOL_OFF_SLAB;

  block_size = ROUND_UP(size, align);

  // Calculate the optimal slab capacity trying to 
  for (slab_page_order = 0; ; slab_page_order++) {
    size_t total = (PAGE_SIZE << slab_page_order);
    size_t extra, extra_per_block;

    if (slab_page_order > PAGE_ORDER_MAX)
      return -EINVAL;

    if (flags & OBJECT_POOL_OFF_SLAB) {
      extra = 0;
      extra_per_block = 0;
    } else {
      extra = sizeof(struct ObjectSlab);
      extra_per_block = sizeof(struct ObjectTag);
    }

    slab_capacity = (total - extra) / (block_size + extra_per_block);

    wastage = total - slab_capacity * (block_size + extra_per_block) - extra;
    if ((wastage * 8) <= total)
      break;
  }

  spin_init(&cache->lock, name);

  list_init(&cache->slabs_empty);
  list_init(&cache->slabs_partial);
  list_init(&cache->slabs_full);

  cache->flags           = flags;
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

static struct ObjectTag *
object_to_tag(struct ObjectSlab *slab, void *obj)
{
  int i = ((uintptr_t) obj - (uintptr_t) slab->data) / slab->pool->block_size;
  return &slab->tags[i];
}

static void *
tag_to_object(struct ObjectSlab *slab, struct ObjectTag *tag)
{
  int i = tag - slab->tags;
  return (uint8_t *) slab->data + slab->pool->block_size * i;
}

static struct ObjectSlab *
object_pool_slab_create(struct ObjectPool *pool)
{
  struct ObjectSlab *slab;
  struct Page *page;
  size_t extra_size;
  uint8_t *data, *end, *p;
  unsigned i;

  assert(spin_holding(&pool->lock));

  if ((page = page_alloc_block(pool->slab_page_order, 0)) == NULL)
    return NULL;

  data = (uint8_t *) page2kva(page);
  end  = data + (PAGE_SIZE << pool->slab_page_order);

  extra_size = sizeof(struct ObjectSlab);
  extra_size += (pool->slab_capacity * sizeof(struct ObjectTag));

  if (pool->flags & OBJECT_POOL_OFF_SLAB) {
    if ((slab = (struct ObjectSlab *) kmalloc(extra_size)) == NULL) {
      page_free_block(page, pool->slab_page_order);
      return NULL;
    }
  } else {
    end -= extra_size;
    slab = (struct ObjectSlab *) end;
  }

  for (i = 0; i < (1U << pool->slab_page_order); i++)
    page[i].slab = slab;
  page->ref_count++;

  data += pool->color_next;

  // Calculate the next color offset.
  pool->color_next += pool->block_align;
  if (pool->color_next > pool->color_max)
    pool->color_next = 0;

  slab->data       = data;
  slab->pool       = pool;
  slab->used_count = 0;
  slab->free       = NULL;

  p = data;
  for (i = 0; i < pool->slab_capacity; i++) {
    struct ObjectTag *tag = object_to_tag(slab, p);

    tag->next  = slab->free;
    slab->free = tag;

    if (pool->obj_ctor)
      pool->obj_ctor(p, pool->obj_size);

    p += pool->block_size;
    
    assert((void *) p <= (void *) end);
  }

  list_add_back(&pool->slabs_full, &slab->link);

  return slab;
}

static void
object_pool_slab_destroy(struct ObjectSlab *slab)
{
  struct ObjectPool *pool = slab->pool;
  struct Page *page;
  unsigned i;
  
  assert(spin_holding(&slab->pool->lock));
  assert(slab->used_count == 0);

  // Call destructor for all objects
  if (slab->pool->obj_dtor != NULL) {
    struct ObjectTag *tag;

    for (tag = slab->free; tag != NULL; tag = tag->next)
      pool->obj_dtor(tag_to_object(slab, tag), pool->obj_size);
  }

  // Free the page containing the data
  page = kva2page(slab->data);

  for (i = 0; i < (1U << pool->slab_page_order); i++)
    page[i].slab = NULL;

  if (pool->flags & OBJECT_POOL_OFF_SLAB)
    kfree(slab);

  page->ref_count--;
  page_free_block(page, pool->slab_page_order);
}

static void *
object_pool_slab_get(struct ObjectSlab *slab)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectTag *tag;

  assert(slab->used_count < pool->slab_capacity);
  assert(slab->free != NULL);

  tag = slab->free;
  slab->free = tag->next;
  slab->used_count++;

  if (slab->used_count == pool->slab_capacity) {
    assert(slab->free == NULL);

    list_remove(&slab->link);
    list_add_back(&pool->slabs_empty, &slab->link);
  }

  return tag_to_object(slab, tag);
}

static void
object_pool_slab_put(struct ObjectSlab *slab, void *obj)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectTag *tag;
  
  assert(slab->used_count > 0);

  tag = object_to_tag(slab, obj);

  tag->next = slab->free;
  slab->free = tag;

  slab->used_count--;

  if (slab->used_count == 0) {
    list_remove(&slab->link);
    list_add_front(&pool->slabs_full, &slab->link);
  } else if (slab->used_count == pool->slab_capacity - 1) {
    list_remove(&slab->link);
    list_add_front(&pool->slabs_partial, &slab->link);
  }
}
