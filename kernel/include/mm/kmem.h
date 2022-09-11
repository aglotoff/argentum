#ifndef __KERNEL_MM_KMEM_H__
#define __KERNEL_MM_KMEM_H__

/**
 * @file kernel/include/mm/kmem.h
 * 
 * Kernel Object Allocator.
 */

#include <list.h>
#include <sync.h>

/**
 * Object cache descriptor.
 */
struct KMemCache {
  /** Spinlock protecting the cache. */
  struct SpinLock   lock;

  /** Empty slabs (all buffers allocated). */
  struct ListLink   slabs_empty;
  /** Partial slabs (some buffers allocated, some free). */
  struct ListLink   slabs_partial;
  /** Complete slabs (all buffers free). */
  struct ListLink   slabs_full;

  /** The number of object per one slab. */
  unsigned          slab_capacity;
  /** Page block order for each slab. */
  unsigned          slab_page_order;

  /** Single of a single object. */
  size_t            obj_size;
  /** Object alignment. */
  size_t            obj_align;
  /** Function to construct objects in the cache. */
  void            (*obj_ctor)(void *, size_t);
  /** Function to undo object construction. */
  void            (*obj_dtor)(void *, size_t);

  /** The maximum slab color offset. */
  size_t            color_max;
  /** The color offset to be used by the next slab. */
  size_t            color_next;

  /** Link into the global list of cache descriptors. */
  struct ListLink   link;

  /** Human-readable cache name (for debugging purposes). */
  const char       *name;

  int flags;
};

enum {
  KMEM_CACHE_OFFSLAB = (1 << 0),    ///< Keep descriptors off-slab
};

struct KMemBufCtl {
  struct KMemBufCtl *next;
};

/**
 * Object slab descriptor.
 */
struct KMemSlab {
  struct ListLink    link;        
  void              *buf;       
  struct KMemBufCtl *free;         
  unsigned           in_use;
};

struct KMemCache *kmem_cache_create(const char *,
                                    size_t, size_t,
                                    void (*)(void *, size_t),
                                    void (*)(void *, size_t));
int               kmem_cache_destroy(struct KMemCache *);

void             *kmem_alloc(struct KMemCache *);
void              kmem_free(struct KMemCache *, void *);

void              kmem_init(void);

#endif  // !__KERNEL_MM_KMEM_H__
