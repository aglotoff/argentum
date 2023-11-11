#ifndef __AG_INCLUDE_KERNEL_OBJECT_H__
#define __AG_INCLUDE_KERNEL_OBJECT_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <kernel/list.h>
#include <kernel/spinlock.h>

#define OBJECT_CACHE_NAME_MAX   64

/**
 * Object cache descriptor.
 */
struct ObjectCache {
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

  /** Size of a single buffer. */
  size_t            buf_size;
  /** Buffer alignment. */
  size_t            buf_align;

  /** Single of a single object. */
  size_t            obj_size;
  /** Function to construct objects in the cache. */
  void            (*ctor)(void *, size_t);
  /** Function to undo object construction. */
  void            (*dtor)(void *, size_t);

  /** The maximum slab color offset. */
  size_t            color_max;
  /** The color offset to be used by the next slab. */
  size_t            color_next;

  /** Link into the global list of cache descriptors. */
  struct ListLink   link;

  /** Human-readable cache name (for debugging purposes). */
  char              name[OBJECT_CACHE_NAME_MAX + 1];
};

struct ObjectBufCtl {
  struct ObjectBufCtl *next;
};

/**
 * Object slab descriptor.
 */
struct ObjectSlab {
  /** Linkage in the cache. */
  struct ListLink      link;
  /** Address of the first buffer in the slab. */
  void                *buf;
  /** List of free buffers. */
  struct ObjectBufCtl *free;  
  /** Reference count for allocated buffers. */       
  unsigned             in_use;
};

struct ObjectCache *object_cache_create(const char *, size_t, size_t,
                                        void (*)(void *, size_t),
                                        void (*)(void *, size_t));
int                 object_cache_destroy(struct ObjectCache *);
void               *object_alloc(struct ObjectCache *);
void                object_free(struct ObjectCache *, void *);
void                object_init(void);

#endif  // !__AG_INCLUDE_KERNEL_OBJECT_H__
