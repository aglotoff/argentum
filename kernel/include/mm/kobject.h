#ifndef __KERNEL_MM_KOBJECT_H__
#define __KERNEL_MM_KOBJECT_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <list.h>
#include <sync.h>

/**
 * Object pool descriptor.
 */
struct KObjectPool {
  struct ListLink   slabs_used;       ///< Slabs with all objects in use
  struct ListLink   slabs_partial;    ///< Slabs that have free objects
  struct ListLink   slabs_free;       ///< Slabs with all objects free
  struct SpinLock   lock;             ///< Spinlock protecting the pool

  int               flags;            ///< Flags
  size_t            obj_size;         ///< Size of each object
  unsigned          obj_num;          ///< The number of objects per slab
  unsigned          page_order;       ///< log2 of the slab size in pages

  size_t            color_offset;     ///< The number of different color lines
  size_t            color_align;      ///< Object alignment in the slab
  size_t            color_next;       ///< The next color offset to use

  const char       *name;             ///< Human-readable name for debugging
  struct ListLink   link;             ///< Link into the pool list
};

enum {
  KOBJECT_POOL_OFFSLAB = (1 << 0),    ///< Keep descriptors off-slab
};

struct KObjectNode {
  struct KObjectNode *next;
};

/**
 * Object slab descriptor.
 */
struct KObjectSlab {
  struct ListLink     link;           ///< Link into the containing slab list
  void               *buf;            ///< Starting address of 
  struct KObjectNode *free;           ///< Pointer to the list of free objects
  unsigned            in_use;         ///< The number of objects in use
};

struct KObjectPool *kobject_pool_create(const char *, size_t, size_t);
int                 kobject_pool_destroy(struct KObjectPool *);

void               *kobject_alloc(struct KObjectPool *);
void                kobject_free(struct KObjectPool *, void *);

void                kobject_pool_init(void);
void                kobject_pool_info(void);

void
kobject_dump(struct KObjectPool *pool);

#endif  // !__KERNEL_MM_KOBJECT_H__
