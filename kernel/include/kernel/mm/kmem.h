#ifndef __KERNEL_INCLUDE_KERNEL_MM_KMEM_H__
#define __KERNEL_INCLUDE_KERNEL_MM_KMEM_H__

/**
 * @file include/mm/kmem.h
 * 
 * Kernel Object Allocator.
 */

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/list.h>
#include <kernel/spinlock.h>

#define KMEM_CACHE_NAME_MAX   64

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

  /** Single of a single buffer. */
  size_t            buf_size;
  /** Buffer alignment alignment. */
  size_t            buf_align;

  /** Single of a single object. */
  size_t            obj_size;
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
  char              name[KMEM_CACHE_NAME_MAX + 1];
};

struct KMemBufCtl {
  struct KMemBufCtl *next;
};

/**
 * Object slab descriptor.
 */
struct KMemSlab {
  /** Linkage in the cache. */
  struct ListLink    link;
  /** Address of the first buffer in the slab. */
  void              *buf;
  /** List of free buffers. */
  struct KMemBufCtl *free;  
  /** Reference count for allocated buffers. */       
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

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_KMEM_H__
