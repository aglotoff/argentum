#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <kernel/assert.h>
#include <kernel/cprintf.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>
#include <kernel/types.h>

/**
 * @defgroup object_pool Object Memory Allocator
 * 
 * Overview
 * --------
 * 
 * The kernel maintains the list of pools for frequently allocated and freed
 * fixed-size objects. Having a dedicated pool for each type of kernel objects
 * also allows to preserve the invariant portion of the object's state thus
 * reducing the allocation time.
 * 
 * General-purpose allocation routines ('kmalloc' and 'kfree') are implemented
 * using an internal set of "anonymous" pools of various predefined sizes.
 * 
 * Implementation
 * --------------
 * 
 * This implementation is based on the paper "The Slab Allocator: An
 * Object-Caching Kernel Memory Allocator" by Jeff Bonwick with the following
 * differences (some of which are borrowed from the Linux kernel):
 * 1. We use the term "object pool" rather than "object cache" to eliminate
 *    ambiguity with other parts of the kernel, e.q. the block cache.
 * 2. Instead of 'kmem_bufctl' structures, for each slab we have an array of
 *    "object tags" that contain only free list linkage and use simple indices
 *    for mapping objects to corresponding tags and vice versa. For off-slab
 *    structures, we allocate the slab descriptor and the tags array from an
 *    anonymous cache of the most suitable size.
 * 3. For each allocated page, we store the pointer to the slab descriptor in
 *    the corresponding page descriptor so given an object pointer we can easily
 *    determine the slab (and the pool) this object belongs to. This eliminates
 *    the need to have a per-cache hash table for mapping objects to bufctls.
 *
 * For more info on the slab allocator, see the original paper.
 */

static int                object_pool_init(struct ObjectPool *, const char *,
                                           size_t, size_t,
                                           void (*)(void *, size_t),
                                           void (*)(void *, size_t));
static struct ObjectSlab *object_pool_slab_create(struct ObjectPool *);
static void               object_pool_slab_destroy(struct ObjectSlab *);
static void              *object_pool_slab_get(struct ObjectSlab *);
static void               object_pool_slab_put(struct ObjectSlab *, void *);                            

/** Linked list to keep track of all object pools in the system */
static struct {
  struct ListLink head;
  struct SpinLock lock;
} pool_list = {
  LIST_INITIALIZER(pool_list.head),
  SPIN_INITIALIZER("pool_list"),
};

/** Pool of pool descriptors */
static struct ObjectPool pool_of_pools;

// TODO: maybe there is a better sequence of sizes rather than just powers of 2
#define ANON_POOLS_LENGTH     12
#define ANON_POOLS_MIN_SIZE   8U
#define ANON_POOLS_MAX_SIZE   (ANON_POOLS_MIN_SIZE << (ANON_POOLS_LENGTH - 1))

/** Set of anonymous pools to be used by kmalloc */
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
                   void (*ctor)(void *, size_t),
                   void (*dtor)(void *, size_t))
{
  struct ObjectPool *pool;
  
  if ((pool = object_pool_get(&pool_of_pools)) == NULL)
    return NULL;

  if (object_pool_init(pool, name, size, align, ctor, dtor) < 0) {
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
  if (pool == &pool_of_pools)
    panic("trying to destroy the pool of pools");

  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_empty) || !list_empty(&pool->slabs_partial)) {
    spin_unlock(&pool->lock);
    return -EBUSY;
  }

  // Destroy all slabs
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

/**
 * Allocate an object from the pool.
 * 
 * @param pool Pointer to the pool descriptor to allocate from.
 * @return The allocated object of NULL if out of memory.
 */
void *
object_pool_get(struct ObjectPool *pool)
{
  struct ObjectSlab *slab;
  void *obj;
  
  spin_lock(&pool->lock);

  // First, try to use partially full slabs
  if (!list_empty(&pool->slabs_partial)) {
    slab = LIST_CONTAINER(pool->slabs_partial.next, struct ObjectSlab, link);
  } else {
    // Then full slabs
    if (!list_empty(&pool->slabs_full)) {
      slab = LIST_CONTAINER(pool->slabs_full.next, struct ObjectSlab, link);
    // Then try to allocate a new slab
    } else if ((slab = object_pool_slab_create(pool)) == NULL) {
      spin_unlock(&pool->lock);
      return NULL;
    }

    // Put the selected slab into the partial list. object_pool_slab_get() will
    // put it into the empty list later, if necessary
    list_remove(&slab->link);
    list_add_back(&pool->slabs_partial, &slab->link);
  } 

  obj = object_pool_slab_get(slab);

  spin_unlock(&pool->lock);

  return obj;
}

/**
 * Return a previously allocated object into the pool.
 * 
 * @param pool Pointer to the pool descriptor
 * @param obj  Pointer to the object to be deallocated
 */
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

/**
 * Initialize the object pool system. This must be called only after the page
 * allocator has been initialized.
 */
void
system_object_pool_init(void)
{ 
  int i;

  // First, solve the "chicken and egg" problem by initializing the static
  // pool of pool descriptors
  if (object_pool_init(&pool_of_pools, "pool_of_pools",
                       sizeof(struct ObjectPool), 0, NULL, NULL) < 0)
    panic("cannot initialize pool_of_pools");

  // Then, initialize the set of anonymous pools used by kmalloc and kfree
  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t size = ANON_POOLS_MIN_SIZE << i;
    char name[OBJECT_POOL_NAME_MAX];

    snprintf(name, sizeof(name), "anon(%u)", size);

    anon_pools[i] = object_pool_create(name, size, 0, NULL, NULL);
    if (anon_pools[i] == NULL)
      panic("cannot initialize %s", name);
  }
}

/**
 * General-purpose kernel memory allocator. Use for (relatively) small memory
 * allocations when the physical page allocator is unsuitable but creating a
 * dedicated object pool is also not feasible.
 * 
 * @param size The number of bytes to allocate
 * @return Pointer to the allocated block of memory or NULL if out of memory
 */
void *
kmalloc(size_t size)
{
  int i;

  // Search the anonymous pool of smallest suitable size
  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t pool_size = ANON_POOLS_MIN_SIZE << i;

    if (size <= pool_size)
      return object_pool_get(anon_pools[i]);
  }

  return NULL;
}

/**
 * Deallocate a block of memory previously allocated by 'kmalloc'.
 * 
 * @param prt Pointer to the memory to be freed
 */
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

/**
 * Initialize a (statically) allocated object pool.
 * 
 * @param pool Pointer to the descriptor to be initialized
 * @param name  Identifies the pool for statistics and debugging
 * @param size  The size of each object in bytes
 * @param align The alignment of each object (or 0 if no special alignment is
 *              required).
 * @param ctor  Function to construct objects in the pool (or NULL)
 * @param dtor  Function to undo object construction in the pool (or NULL)
 * 
 * @return 0 on success, an error code otherwise
 */
static int
object_pool_init(struct ObjectPool *pool,
                 const char *name,
                 size_t size,
                 size_t align,
                 void (*ctor)(void *, size_t),
                 void (*dtor)(void *, size_t))
{
  size_t wastage, block_size;
  unsigned slab_page_order, slab_capacity;
  int flags;

  if (size < align)
    return -EINVAL;
  if ((PAGE_SIZE % align) != 0)
    return -EINVAL;

  align = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);

  // To reduce wastage, store data structures for large allocations off-slab
  flags = 0;
  if (size > (PAGE_SIZE / 8))
    flags |= OBJECT_POOL_OFF_SLAB;

  block_size = ROUND_UP(size, align);

  // Calculate the optimal slab capacity trying to keep the size of unused space
  // under 12.5% of total memory
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

  spin_init(&pool->lock, name);

  list_init(&pool->slabs_empty);
  list_init(&pool->slabs_partial);
  list_init(&pool->slabs_full);

  pool->flags           = flags;
  pool->slab_capacity   = slab_capacity;
  pool->slab_page_order = slab_page_order;
  pool->block_size      = block_size;
  pool->block_align     = align;
  pool->obj_size        = size;
  pool->obj_ctor        = ctor;
  pool->obj_dtor        = dtor;
  pool->color_max       = wastage;
  pool->color_next      = 0;

  spin_lock(&pool_list.lock);
  list_add_back(&pool_list.head, &pool->link);
  spin_unlock(&pool_list.lock);

  strncpy(pool->name, name, OBJECT_POOL_NAME_MAX);
  pool->name[OBJECT_POOL_NAME_MAX] = '\0';

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

/**
 * Create a new slab for the given object pool.
 * 
 * @param pool Pointer to the pool descritptor
 * @return Pointer to the slab descriptor, or NULL if out of memory
 */
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

  // Store the pointer to the slab descriptor into each page descriptor to mark
  // the corresponding pages as used by this slab
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

  // Initialize all objects in the slab
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

  // Add the newly allocated slab to the full list
  // object_pool_alloc will move it to the partial list
  list_add_back(&pool->slabs_full, &slab->link);

  return slab;
}

/**
 * Destroy the slab and all its objects
 * 
 * @param slab Pointer to the slab descriptor
 */
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

  // Free the page block
  page = kva2page(slab->data);

  for (i = 0; i < (1U << pool->slab_page_order); i++) {
    if (page[i].slab != slab)
      panic("trying to free a page that doesn't belong to slab");
    page[i].slab = NULL;
  }

  if (pool->flags & OBJECT_POOL_OFF_SLAB)
    kfree(slab);

  page->ref_count--;
  page_free_block(page, pool->slab_page_order);
}

/**
 * Allocate a single object from the slab. The slab must not be empty.
 * 
 * @param slab Pointer to the slab descriptor
 * @return Pointer to the allocated object
 */
static void *
object_pool_slab_get(struct ObjectSlab *slab)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectTag *tag;

  // The slab must be on the partial list (even if it is full, the
  // object_pool_alloc function moves it to the partial list before calling this
  // function)
  assert(slab->used_count < pool->slab_capacity);
  assert(slab->free != NULL);

  tag = slab->free;
  slab->free = tag->next;
  slab->used_count++;

  // The slab becomes empty: move it into the corresponding list
  if (slab->used_count == pool->slab_capacity) {
    assert(slab->free == NULL);

    list_remove(&slab->link);
    list_add_back(&pool->slabs_empty, &slab->link);
  }

  return tag_to_object(slab, tag);
}

/**
 * Return a previously allocated object to the slab.
 * 
 * @param slab Pointer to the slab descriptor
 * @param obj Pointer to the object to be deallocated
 */
static void
object_pool_slab_put(struct ObjectSlab *slab, void *obj)
{
  struct ObjectPool *pool = slab->pool;
  struct ObjectTag *tag;
  
  assert(slab->used_count > 0);

  // TODO: panic if the object doesn't belong to this slab

  tag = object_to_tag(slab, obj);

  tag->next = slab->free;
  slab->free = tag;

  slab->used_count--;

  if (slab->used_count == 0) {
    // Slab becomes full
    list_remove(&slab->link);
    list_add_front(&pool->slabs_full, &slab->link);
  } else if (slab->used_count == pool->slab_capacity - 1) {
    // Slab becomes partially full
    list_remove(&slab->link);
    list_add_front(&pool->slabs_partial, &slab->link);
  }
}
