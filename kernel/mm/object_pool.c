#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <kernel/core/assert.h>
#include <kernel/console.h>
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
 * General-purpose allocation routines ('k_malloc' and 'k_free') are implemented
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

static int                 k_object_pool_init(struct KObjectPool *, const char *,
                                              size_t, size_t,
                                              void (*)(void *, size_t),
                                              void (*)(void *, size_t));
static struct KObjectSlab *k_object_pool_slab_create(struct KObjectPool *);
static void                k_object_pool_slab_destroy(struct KObjectSlab *);
static void               *k_object_pool_slab_get(struct KObjectSlab *);
static void                k_object_pool_slab_put(struct KObjectSlab *, void *);                            

/** Linked list to keep track of all object pools in the system */
static struct {
  struct KListLink head;
  struct KSpinLock lock;
} pool_list = {
  K_LIST_INITIALIZER(pool_list.head),
  K_SPINLOCK_INITIALIZER("pool_list"),
};

/** Pool of pool descriptors */
static struct KObjectPool pool_of_pools;

// TODO: maybe there is a better sequence of sizes rather than just powers of 2
#define ANON_POOLS_LENGTH     12
#define ANON_POOLS_MIN_SIZE   8U
#define ANON_POOLS_MAX_SIZE   (ANON_POOLS_MIN_SIZE << (ANON_POOLS_LENGTH - 1))

/** Set of anonymous pools to be used by k_malloc */
static struct KObjectPool *anon_pools[ANON_POOLS_LENGTH];

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
struct KObjectPool *
k_object_pool_create(const char *name,
                   size_t size,
                   size_t align,
                   void (*ctor)(void *, size_t),
                   void (*dtor)(void *, size_t))
{
  struct KObjectPool *pool;
  
  if ((pool = k_object_pool_get(&pool_of_pools)) == NULL)
    return NULL;

  if (k_object_pool_init(pool, name, size, align, ctor, dtor) < 0) {
    k_object_pool_put(&pool_of_pools, pool);
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
k_object_pool_destroy(struct KObjectPool *pool)
{
  if (pool == &pool_of_pools)
    k_panic("trying to destroy the pool of pools");

  k_spinlock_acquire(&pool->lock);

  if (!k_list_is_empty(&pool->slabs_empty) || !k_list_is_empty(&pool->slabs_partial)) {
    k_spinlock_release(&pool->lock);
    return -EBUSY;
  }

  // Destroy all slabs
  while (!k_list_is_empty(&pool->slabs_full)) {
    struct KObjectSlab *slab;

    slab = K_LIST_CONTAINER(pool->slabs_full.next, struct KObjectSlab, link);
    k_list_remove(&slab->link);

    k_object_pool_slab_destroy(slab);
  }

  k_spinlock_release(&pool->lock);

  k_spinlock_acquire(&pool_list.lock);
  k_list_remove(&pool->link);
  k_spinlock_release(&pool_list.lock);

  k_object_pool_put(&pool_of_pools, pool);

  return 0;
}

/**
 * Allocate an object from the pool.
 * 
 * @param pool Pointer to the pool descriptor to allocate from.
 * @return The allocated object of NULL if out of memory.
 */
void *
k_object_pool_get(struct KObjectPool *pool)
{
  struct KObjectSlab *slab;
  void *obj;
  
  k_spinlock_acquire(&pool->lock);

  // First, try to use partially full slabs
  if (!k_list_is_empty(&pool->slabs_partial)) {
    slab = K_LIST_CONTAINER(pool->slabs_partial.next, struct KObjectSlab, link);
  } else {
    // Then full slabs
    if (!k_list_is_empty(&pool->slabs_full)) {
      slab = K_LIST_CONTAINER(pool->slabs_full.next, struct KObjectSlab, link);
    // Then try to allocate a new slab
    } else if ((slab = k_object_pool_slab_create(pool)) == NULL) {
      k_spinlock_release(&pool->lock);
      k_panic("%s: out of memory\n", pool->name);
      return NULL;
    }

    // Put the selected slab into the partial list. k_object_pool_slab_get() will
    // put it into the empty list later, if necessary
    k_list_remove(&slab->link);
    k_list_add_back(&pool->slabs_partial, &slab->link);
  } 

  obj = k_object_pool_slab_get(slab);

  k_spinlock_release(&pool->lock);

  return obj;
}

/**
 * Return a previously allocated object into the pool.
 * 
 * @param pool Pointer to the pool descriptor
 * @param obj  Pointer to the object to be deallocated
 */
void
k_object_pool_put(struct KObjectPool *pool, void *obj)
{
  struct Page *page;
  struct KObjectSlab *slab;

  k_spinlock_acquire(&pool->lock);

  page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << pool->slab_page_order));
  page_assert(page, pool->slab_page_order, PAGE_TAG_SLAB);
  slab = page->slab;

  k_object_pool_slab_put(slab, obj);

  k_spinlock_release(&pool->lock);
}

/**
 * Initialize the object pool system. This must be called only after the page
 * allocator has been initialized.
 */
void
k_object_pool_system_init(void)
{ 
  int i;

  // First, solve the "chicken and egg" problem by initializing the static
  // pool of pool descriptors
  if (k_object_pool_init(&pool_of_pools, "pool_of_pools",
                       sizeof(struct KObjectPool), 0, NULL, NULL) < 0)
    k_panic("cannot initialize pool_of_pools");

  // Then, initialize the set of anonymous pools used by k_malloc and k_free
  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t size = ANON_POOLS_MIN_SIZE << i;
    char name[K_OBJECT_POOL_NAME_MAX];

    snprintf(name, sizeof(name), "anon(%u)", (unsigned) size);

    anon_pools[i] = k_object_pool_create(name, size, 0, NULL, NULL);
    if (anon_pools[i] == NULL)
      k_panic("cannot initialize %s", name);
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
k_malloc(size_t size)
{
  int i;

  // Search the anonymous pool of smallest suitable size
  for (i = 0; i < ANON_POOLS_LENGTH; i++) {
    size_t pool_size = ANON_POOLS_MIN_SIZE << i;

    if (size <= pool_size)
      return k_object_pool_get(anon_pools[i]);
  }

  return NULL;
}

/**
 * Deallocate a block of memory previously allocated by 'k_malloc'.
 * 
 * @param prt Pointer to the memory to be freed
 */
void
k_free(void *ptr)
{
  struct Page *page;

  // Determine the slab (and the pool) this pointer belongs to
  page = kva2page(ptr);
  if (page->slab == NULL)
    k_panic("bad pointer");

  k_object_pool_put(page->slab->pool, ptr);
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
k_object_pool_init(struct KObjectPool *pool,
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
  if (align && (PAGE_SIZE % align) != 0)
    return -EINVAL;

  align = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);

  // To reduce wastage, store data structures for large allocations off-slab
  flags = 0;
  if (size > (PAGE_SIZE / 8))
    flags |= K_OBJECT_POOL_OFF_SLAB;

  block_size = ROUND_UP(size, align);

  // Calculate the optimal slab capacity trying to keep the size of unused space
  // under 12.5% of total memory
  for (slab_page_order = 0; ; slab_page_order++) {
    size_t total = (PAGE_SIZE << slab_page_order);
    size_t extra, extra_per_block;

    if (slab_page_order > PAGE_ORDER_MAX)
      return -EINVAL;

    if (flags & K_OBJECT_POOL_OFF_SLAB) {
      extra = 0;
      extra_per_block = 0;
    } else {
      extra = sizeof(struct KObjectSlab);
      extra_per_block = sizeof(struct KObjectTag);
    }

    slab_capacity = (total - extra) / (block_size + extra_per_block);

    wastage = total - slab_capacity * (block_size + extra_per_block) - extra;
    if ((wastage * 8) <= total)
      break;
  }

  k_spinlock_init(&pool->lock, name);

  k_list_init(&pool->slabs_empty);
  k_list_init(&pool->slabs_partial);
  k_list_init(&pool->slabs_full);

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

  k_spinlock_acquire(&pool_list.lock);
  k_list_add_back(&pool_list.head, &pool->link);
  k_spinlock_release(&pool_list.lock);

  strncpy(pool->name, name, K_OBJECT_POOL_NAME_MAX);
  pool->name[K_OBJECT_POOL_NAME_MAX] = '\0';

  return 0;
}

static struct KObjectTag *
object_to_tag(struct KObjectSlab *slab, void *obj)
{
  int i = ((uintptr_t) obj - (uintptr_t) slab->data) / slab->pool->block_size;
  return &slab->tags[i];
}

static void *
tag_to_object(struct KObjectSlab *slab, struct KObjectTag *tag)
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
static struct KObjectSlab *
k_object_pool_slab_create(struct KObjectPool *pool)
{
  struct KObjectSlab *slab;
  struct Page *page;
  size_t extra_size;
  uint8_t *data, *end, *p;
  unsigned i;

  k_assert(k_spinlock_holding(&pool->lock));

  if ((page = page_alloc_block(pool->slab_page_order, 0, PAGE_TAG_SLAB)) == NULL)
    return NULL;

  data = (uint8_t *) page2kva(page);
  end  = data + (PAGE_SIZE << pool->slab_page_order);

  extra_size = sizeof(struct KObjectSlab);
  extra_size += (pool->slab_capacity * sizeof(struct KObjectTag));

  if (pool->flags & K_OBJECT_POOL_OFF_SLAB) {
    if ((slab = (struct KObjectSlab *) k_malloc(extra_size)) == NULL) {
      page_free_block(page, pool->slab_page_order);
      return NULL;
    }
  } else {
    end -= extra_size;
    slab = (struct KObjectSlab *) end;
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
  slab->link.prev  = NULL;
  slab->link.next  = NULL;

  // Initialize all objects in the slab
  p = data;
  for (i = 0; i < pool->slab_capacity; i++) {
    struct KObjectTag *tag = object_to_tag(slab, p);

    tag->next  = slab->free;
    slab->free = tag;

    if (pool->obj_ctor)
      pool->obj_ctor(p, pool->obj_size);

    p += pool->block_size;
    
    k_assert((void *) p <= (void *) end);
  }

  // Add the newly allocated slab to the full list
  // object_pool_alloc will move it to the partial list
  k_list_add_back(&pool->slabs_full, &slab->link);

  return slab;
}

/**
 * Destroy the slab and all its objects
 * 
 * @param slab Pointer to the slab descriptor
 */
static void
k_object_pool_slab_destroy(struct KObjectSlab *slab)
{
  struct KObjectPool *pool = slab->pool;
  struct Page *page;
  unsigned i;
  
  k_assert(k_spinlock_holding(&slab->pool->lock));
  k_assert(slab->used_count == 0);

  // Call destructor for all objects
  if (slab->pool->obj_dtor != NULL) {
    struct KObjectTag *tag;

    for (tag = slab->free; tag != NULL; tag = tag->next)
      pool->obj_dtor(tag_to_object(slab, tag), pool->obj_size);
  }

  // Free the page block
  page = kva2page(slab->data);
  page_assert(page, pool->slab_page_order, PAGE_TAG_SLAB);

  for (i = 0; i < (1U << pool->slab_page_order); i++) {
    if (page[i].slab != slab)
      k_panic("trying to free a page that doesn't belong to slab");
    page[i].slab = NULL;
  }

  if (pool->flags & K_OBJECT_POOL_OFF_SLAB)
    k_free(slab);

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
k_object_pool_slab_get(struct KObjectSlab *slab)
{
  struct KObjectPool *pool = slab->pool;
  struct KObjectTag *tag;

  page_assert(kva2page(slab->data), pool->slab_page_order, PAGE_TAG_SLAB);

  // The slab must be on the partial list (even if it is full, the
  // object_pool_alloc function moves it to the partial list before calling this
  // function)
  k_assert(slab->used_count < pool->slab_capacity);
  k_assert(slab->free != NULL);

  tag = slab->free;
  slab->free = tag->next;
  slab->used_count++;

  // The slab becomes empty: move it into the corresponding list
  if (slab->used_count == pool->slab_capacity) {
    k_assert(slab->free == NULL);

    k_list_remove(&slab->link);
    k_list_add_back(&pool->slabs_empty, &slab->link);
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
k_object_pool_slab_put(struct KObjectSlab *slab, void *obj)
{
  struct KObjectPool *pool = slab->pool;
  struct KObjectTag *tag;
  
  k_assert(slab->used_count > 0);

  page_assert(kva2page(slab->data), pool->slab_page_order, PAGE_TAG_SLAB);

  // TODO: panic if the object doesn't belong to this slab

  tag = object_to_tag(slab, obj);

  tag->next = slab->free;
  slab->free = tag;

  slab->used_count--;

  if (slab->used_count == 0) {
    // Slab becomes full
    k_list_remove(&slab->link);
    k_list_add_front(&pool->slabs_full, &slab->link);
  } else if (slab->used_count == pool->slab_capacity - 1) {
    // Slab becomes partially full
    k_list_remove(&slab->link);
    k_list_add_front(&pool->slabs_partial, &slab->link);
  }
}
