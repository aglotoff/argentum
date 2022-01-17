#include <assert.h>
#include <errno.h>
#include <string.h>

#include "console.h"
#include "kernel.h"
#include "kobject.h"
#include "page.h"
#include "sync.h"

// static int                 kobject_pool_grow(struct KObjectPool *);
static unsigned            kobject_pool_estimate(size_t, unsigned, int,
                                                 size_t *);
static struct KObjectSlab *kobject_slab_alloc(struct KObjectPool *);
static void                kobject_slab_destroy(struct KObjectPool *,
                                                struct KObjectSlab *);

// Linked list of all object pools.
static struct {
  struct ListLink head;
  struct SpinLock lock;
} pool_list = {
  LIST_INITIALIZER(pool_list.head),
  SPIN_INITIALIZER("pool_list"),
};

// Pool for pool descriptors.
static struct KObjectPool pool_pool = {
  .slabs_used    = LIST_INITIALIZER(pool_pool.slabs_used),
  .slabs_partial = LIST_INITIALIZER(pool_pool.slabs_partial),
  .slabs_free    = LIST_INITIALIZER(pool_pool.slabs_free),
  .lock          = SPIN_INITIALIZER("pool_pool"),
  .obj_size      = sizeof(struct KObjectPool),
  .color_align   = sizeof(uintptr_t),
  .name          = "pool_pool",
};

// Pool for off-slab slab descriptors.
static struct KObjectPool slab_pool = {
  .slabs_used    = LIST_INITIALIZER(slab_pool.slabs_used),
  .slabs_partial = LIST_INITIALIZER(slab_pool.slabs_partial),
  .slabs_free    = LIST_INITIALIZER(slab_pool.slabs_free),
  .lock          = SPIN_INITIALIZER("slab_pool"),
  .obj_size      = sizeof(struct KObjectSlab),
  .color_align   = sizeof(uintptr_t),
  .name          = "slab_pool",
};

/*
 * ----------------------------------------------------------------------------
 * Pool manipulation
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Create object pool.
 * 
 * Creates a pool for objects, each size obj_size, aligned on a align
 * boundary.
 * 
 * @param name     Identifies the pool for statistics and debugging.
 * @param obj_size The size of each object in bytes.
 * @param align    The alignment of each object (or 0 if no special alignment)
 *                 is required).
 *
 * @return Pointer to the pool descriptor or NULL if out of memory.
 */
struct KObjectPool *
kobject_pool_create(const char *name, size_t obj_size, size_t align)
{
  struct KObjectPool *pool;
  size_t wastage;
  unsigned page_order, obj_num;
  int flags;

  if ((pool = kobject_alloc(&pool_pool)) == NULL)
    return NULL;

  // Force word size alignment
  align    = align ? ROUND_UP(align, sizeof(uintptr_t)) : sizeof(uintptr_t);
  obj_size = ROUND_UP(obj_size, align);

  // For objects larger that 1/8 of a page, keep descriptors off slab.
  flags = (obj_size >= (PAGE_SIZE / 8)) ? KOBJECT_POOL_OFFSLAB : 0;

  // Estimate slab size
  for (page_order = 0; ; page_order++) {
    if (page_order > PAGE_ORDER_MAX) {
      kobject_free(&pool_pool, pool);
      return NULL;
    }

    // Try to limit internal fragmentation to 12.5% (1/8).
    obj_num = kobject_pool_estimate(obj_size, page_order, flags, &wastage);
    if ((wastage * 8) <= (PAGE_SIZE << page_order))
      break;
  }

  list_init(&pool->slabs_used);
  list_init(&pool->slabs_partial);
  list_init(&pool->slabs_free);

  pool->flags        = flags;
  pool->obj_size     = obj_size;
  pool->obj_num      = obj_num;
  pool->page_order   = page_order;

  pool->color_align  = align;
  pool->color_offset = wastage;
  pool->color_next   = 0;

  spin_init(&pool->lock, name);
  pool->name         = name;

  spin_lock(&pool_list.lock);
  list_add_back(&pool_list.head, &pool->link);
  spin_unlock(&pool_list.lock);

  return pool;
}

//
// Calculate the number of objects that would fit into a slab of size
// 2^page_order pages and how much space would be left over.
//
static unsigned
kobject_pool_estimate(size_t obj_size,
                      unsigned page_order,
                      int flags,
                      size_t *left_over)
{
  size_t wastage;
  unsigned obj_num;

  wastage = (PAGE_SIZE << page_order);
  if (!(flags & KOBJECT_POOL_OFFSLAB))
    wastage -= sizeof(struct KObjectSlab);

  obj_num = wastage / obj_size;

  if (left_over) {
    wastage -= obj_num * obj_size;
    *left_over = wastage;
  }

  return obj_num;
}

/**
 * @brief Destory object cache.
 * 
 * Destroys the cache and reclaims all associated resources.
 * 
 * @param cache The cache descriptor to be destroyed.
 */
int
kobject_pool_destroy(struct KObjectPool *pool)
{
  struct KObjectSlab *slab;
  
  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_used) || !list_empty(&pool->slabs_partial)) {
    spin_unlock(&pool->lock);
    return -EBUSY;
  }

  while(!list_empty(&pool->slabs_free)) {
    slab = LIST_CONTAINER(pool->slabs_free.next, struct KObjectSlab, link);
    list_remove(&slab->link);
    kobject_slab_destroy(pool, slab);
  }

  spin_unlock(&pool->lock);

  spin_lock(&pool_list.lock);
  list_remove(&pool->link);
  spin_unlock(&pool_list.lock);

  kobject_free(&pool_pool, pool);

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * Slab management
 * ----------------------------------------------------------------------------
 */

static struct KObjectSlab *
kobject_slab_alloc(struct KObjectPool *pool)
{
  struct KObjectSlab *slab;
  struct PageInfo *page;
  struct KObjectNode *curr, **prevp;
  unsigned num;
  uint8_t *buf, *p;

  assert(spin_holding(&pool->lock));

  // Allocate page block for the slab.
  if ((page = page_alloc_block(pool->page_order, 0)) == NULL)
    return NULL;

  buf = (uint8_t *) page2kva(page);

  if (pool->flags & KOBJECT_POOL_OFFSLAB) {
    // If slab descriptors are kept off pages, allocate one from the slab pool.
    if ((slab = (struct KObjectSlab *) kobject_alloc(&slab_pool)) == NULL) {
      page_free_block(page, pool->page_order);
      return NULL;
    }
  } else {
    // Otherwise, store the slab descriptor at the end of the page frame.
    slab = (struct KObjectSlab *) (buf + (PAGE_SIZE << pool->page_order)) - 1;
  }

  page->ref_count++;
  page->slab = slab;

  slab->in_use = 0;
  slab->buf = buf;

  // Initialize all objects.
  p = buf + pool->color_next;
  prevp = &slab->free;
  for (num = 0; num < pool->obj_num; num++) {
    curr = (struct KObjectNode *) p;
    curr->next = NULL;

    *prevp = curr;
    prevp = &curr->next;

    p += pool->obj_size;
  }

  // Calculate color offset for the next slab.
  pool->color_next += pool->color_align;
  if (pool->color_next > pool->color_offset)
    pool->color_next = 0;

  return slab;
}

static void
kobject_slab_destroy(struct KObjectPool *pool, struct KObjectSlab *slab)
{
  struct PageInfo *page;
  
  assert(spin_holding(&pool->lock));
  assert(slab->in_use == 0);

  // Free the pages been used for the slab.
  page = kva2page(slab->buf);
  page->ref_count--;
  page_free_block(page, pool->page_order);

  // If slab descriptor was kept off page, return it to the slab pool.
  if (pool->flags & KOBJECT_POOL_OFFSLAB)
    kobject_free(&slab_pool, slab);
}

/*
 * ----------------------------------------------------------------------------
 * Object allocation
 * ----------------------------------------------------------------------------
 */

void *
kobject_alloc(struct KObjectPool *pool)
{
  struct ListLink *list;
  struct KObjectSlab *slab;
  struct KObjectNode *obj;
  
  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_partial)) {
    list = &pool->slabs_partial;
  } else {
    list = &pool->slabs_free;

    if (list_empty(list)) {
      if ((slab = kobject_slab_alloc(pool)) == NULL) {
        spin_unlock(&pool->lock);
        return NULL;
      }

      list_add_back(list, &slab->link);
    }
  }

  slab = LIST_CONTAINER(list->next, struct KObjectSlab, link);

  assert(slab->in_use < pool->obj_num);
  assert(slab->free != NULL);

  if (++slab->in_use == pool->obj_num) {
    list_remove(&slab->link);
    list_add_back(&pool->slabs_used, &slab->link);
  }

  obj = slab->free;
  slab->free = obj->next;

  spin_unlock(&pool->lock);

  return obj;
}

void
kobject_dump(struct KObjectPool *pool)
{
  struct ListLink *list;
  struct KObjectSlab *slab;
   
  spin_lock(&pool->lock);

  if (!list_empty(&pool->slabs_partial)) {
    list = &pool->slabs_partial;
  } else {
    list = &pool->slabs_free;

    if (list_empty(list)) {
      if ((slab = kobject_slab_alloc(pool)) == NULL) {
        spin_unlock(&pool->lock);
        return;
      }

      list_add_back(list, &slab->link);
    }
  }

  slab = LIST_CONTAINER(list->next, struct KObjectSlab, link);

  assert(slab->in_use < pool->obj_num);
  assert(slab->free != NULL);

  spin_unlock(&pool->lock);
}

/*
 * ----------------------------------------------------------------------------
 * Object freeing
 * ----------------------------------------------------------------------------
 */

void
kobject_free(struct KObjectPool *pool, void *obj)
{
  struct PageInfo *slab_page;
  struct KObjectSlab *slab;
  struct KObjectNode *node;

  slab_page = kva2page(ROUND_DOWN(obj, PAGE_SIZE << pool->page_order));
  slab = slab_page->slab;

  spin_lock(&pool->lock);

  node = (struct KObjectNode *) obj;
  node->next = slab->free;
  slab->free = node;

  if (--slab->in_use > 0) {
    list_remove(&slab->link);
    list_add_front(&pool->slabs_partial, &slab->link);
  } else if (slab->in_use == 0) {
    list_remove(&slab->link);
    list_add_front(&pool->slabs_free, &slab->link);
  }

  spin_unlock(&pool->lock);
}

/*
 * ----------------------------------------------------------------------------
 * Initializing the object allocator
 * ----------------------------------------------------------------------------
 */

void
kobject_pool_init(void)
{
  pool_pool.obj_num = kobject_pool_estimate(pool_pool.obj_size,
                                            pool_pool.page_order, 
                                            pool_pool.flags,
                                            &pool_pool.color_offset);
  list_add_back(&pool_list.head, &pool_pool.link);

  slab_pool.obj_num = kobject_pool_estimate(slab_pool.obj_size,
                                            slab_pool.page_order, 
                                            slab_pool.flags,
                                            &slab_pool.color_offset);
  list_add_back(&pool_list.head, &slab_pool.link);
}

void
kobject_pool_info(void)
{
  struct ListLink *link;
  struct KObjectPool *pool;

  LIST_FOREACH(&pool_list.head, link) {
    pool = LIST_CONTAINER(link, struct KObjectPool, link);
    cprintf("%-20s %6d\n", pool->name, pool->obj_size);
  }
}
