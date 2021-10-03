#include <assert.h>

#include "kernel.h"
#include "kobject.h"
#include "page.h"
#include "spinlock.h"

static struct KObjectCache cache_cache;

static void kobject_cache_init(struct KObjectCache *,
                               const char *,
                               size_t,
                               void (*)(void *),
                               void (*)(void *));
static int  kobject_cache_grow(struct KObjectCache *);
static void kobject_cache_shrink(struct KObjectCache *);

// --------------------------------------------------------------
// Cache manipulation
// --------------------------------------------------------------

struct KObjectCache *
kobject_cache_create(const char  *name,
                     size_t       obj_size,
                     void       (*ctor)(void *),
                     void       (*dtor)(void *))
{
  struct KObjectCache *cache;

  obj_size = ROUND_UP(obj_size, sizeof(uintptr_t));
  if (obj_size > (PAGE_SIZE - sizeof(struct KObjectSlab))) {
    warn("too large object size: %d", obj_size);
    return NULL;
  }

  if ((cache = kobject_alloc(&cache_cache)) == NULL)
    return NULL;

  kobject_cache_init(cache, name, obj_size, ctor, dtor);

  return cache;
}

static void
kobject_cache_init(struct KObjectCache *cache,
                   const char          *name,
                   size_t               obj_size,
                   void               (*ctor)(void *),
                   void               (*dtor)(void *))
{
  list_init(&cache->slabs_used);
  list_init(&cache->slabs_partial);
  list_init(&cache->slabs_free);

  cache->obj_size = obj_size;
  cache->obj_num  = (PAGE_SIZE - sizeof(struct KObjectSlab)) / obj_size;

  spin_init(&cache->lock, name);

  assert(cache->obj_num >= 2);

  cache->ctor = ctor;
  cache->dtor = dtor;
  cache->name = name;
}

void
kobject_cache_destroy(struct KObjectCache *cache)
{
  if (!list_empty(&cache->slabs_used) || !list_empty(&cache->slabs_partial))
    panic("object cache has slabs in use");

  kobject_cache_shrink(cache);
  kobject_free(&cache_cache, cache);
}

static int
kobject_cache_grow(struct KObjectCache *cache)
{
  struct PageInfo *page;
  struct KObjectSlab *slab;
  uint8_t *ptr;

  if ((page = page_alloc(PAGE_ALLOC_ZERO)) == NULL)
    return -1;

  page->ref_count++;

  slab = (struct KObjectSlab *) page2kva(page);
  
  for (ptr = (uint8_t *) (slab + 1);
       ptr < (uint8_t *) slab + PAGE_SIZE;
       ptr += cache->obj_size) {
    *(void **) ptr = slab->free;
    slab->free = ptr;
  }

  list_init(&slab->link);
  list_add_front(&cache->slabs_free, &slab->link);

  return 0;
}

static void
kobject_cache_shrink(struct KObjectCache *cache)
{
  struct KObjectSlab *slab;
  struct PageInfo *page;

  while (!list_empty(&cache->slabs_free)) {
    slab = LIST_CONTAINER(cache->slabs_free.next, struct KObjectSlab, link);

    assert(slab->in_use == 0);

    page = kva2page(slab);
    page->ref_count--;
    page_free(page);

    list_remove(&slab->link);
  }
}

// --------------------------------------------------------------
// Object allocation
// --------------------------------------------------------------

void *
kobject_alloc(struct KObjectCache *cache)
{
  struct ListLink *list;
  struct KObjectSlab *slab;
  void *ptr;
  
  spin_lock(&cache->lock);

  if (list_empty(&cache->slabs_partial) && list_empty(&cache->slabs_free) &&
      (kobject_cache_grow(cache) < 0)) {
    spin_unlock(&cache->lock);
    return NULL;
  }
  
  if (!list_empty(&cache->slabs_partial))
    list = &cache->slabs_partial;
  else
    list = &cache->slabs_free;

  slab = LIST_CONTAINER(list->next, struct KObjectSlab, link);

  assert(slab->in_use < cache->obj_num);
  assert(slab->free != NULL);

  if (++slab->in_use == cache->obj_num) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_used, &slab->link);
  }
  
  ptr = slab->free;
  slab->free = *(void **) ptr;

  spin_unlock(&cache->lock);

  return ptr;
}

// --------------------------------------------------------------
// Object freeing
// --------------------------------------------------------------

void
kobject_free(struct KObjectCache *cache, void *obj)
{
  struct KObjectSlab *slab;

  slab = ROUND_DOWN(obj, PAGE_SIZE);

  spin_lock(&cache->lock);

  *(void **) obj = slab->free;
  slab->free = obj;

  if (slab->in_use-- == cache->obj_num) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_partial, &slab->link);
  } else if (slab->in_use == 0) {
    list_remove(&slab->link);
    list_add_front(&cache->slabs_free, &slab->link);
  }

  spin_unlock(&cache->lock);
}

// --------------------------------------------------------------
// Initializing the object allocator
// --------------------------------------------------------------

void
kobject_init(void)
{
  kobject_cache_init(&cache_cache, "cache_cache", sizeof(struct KObjectCache),
                     NULL, NULL);
}
