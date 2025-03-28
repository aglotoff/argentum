#ifndef __KERNEL_OBJECT_POOL_H__
#define __KERNEL_OBJECT_POOL_H__

/**
 * @file include/object_pool.h
 * 
 * Object allocator.
 */

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

#define K_OBJECT_POOL_NAME_MAX  64

/**
 * Object pool descriptor.
 */
struct KObjectPool {
  /** Spinlock protecting this pool. */
  struct KSpinLock  lock;
  /** Various flags */
  int               flags;

  /** Empty slabs (all blocks allocated). */
  struct KListLink   slabs_empty;
  /** Partial slabs (some blocks allocated, some free). */
  struct KListLink   slabs_partial;
  /** Complete slabs (all blocks free). */
  struct KListLink   slabs_full;

  /** The number of object per one slab. */
  unsigned          slab_capacity;
  /** Page block order for each slab. */
  unsigned          slab_page_order;

  /** Size of a single block in bytes. */
  size_t            block_size;
  /** Byte alignment of a single block. */
  size_t            block_align;

  /** Size of a single object in bytes. */
  size_t            obj_size;
  /** Function to construct objects in the pool. */
  void            (*obj_ctor)(void *, size_t);
  /** Function to undo object construction. */
  void            (*obj_dtor)(void *, size_t);

  /** The maximum slab color offset. */
  size_t            color_max;
  /** The color offset to be used by the next slab. */
  size_t            color_next;

  /** Link into the global list of pool descriptors. */
  struct KListLink   link;

  /** Human-readable pool name (for debugging purposes). */
  char              name[K_OBJECT_POOL_NAME_MAX + 1];
};

enum {
  K_OBJECT_POOL_OFF_SLAB = (1 << 0)
};

struct KObjectTag {
  struct KObjectTag *next;
};

/**
 * Object slab descriptor.
 */
struct KObjectSlab {
  /** Linkage in the pool. */
  struct KListLink     link;
  /** The pool this slab belongs to. */
  struct KObjectPool *pool;
  /** Address of the buffer containing all memory blocks. */
  void               *data;
  /** Linked list of free block tags. */
  struct KObjectTag  *free;
  /** Reference count for allocated blocks. */       
  unsigned            used_count;
    /** Address of the bufers containing all block tags. */
  struct KObjectTag   tags[0];
};

struct KObjectPool *k_object_pool_create(const char *, size_t, size_t,
                                      void (*)(void *, size_t),
                                      void (*)(void *, size_t));
int                k_object_pool_destroy(struct KObjectPool *);
void              *k_object_pool_get(struct KObjectPool *);
void               k_object_pool_put(struct KObjectPool *, void *);

void               k_object_pool_system_init(void);

void              *k_malloc(size_t);
void               k_free(void *);

#endif  // !__KERNEL_OBJECT_POOL_H__
