#ifndef __KERNEL_KOBJECT_H__
#define __KERNEL_KOBJECT_H__

#include <list.h>

/**
 * Object cache descriptor.
 */
struct KObjectCache {
  struct ListLink   slabs_used;       ///< Slabs with all objects in use
  struct ListLink   slabs_partial;    ///< Slabs that have free objects
  struct ListLink   slabs_free;       ///< Slabs with all objects free

  size_t            obj_size;         ///< Size of each object
  unsigned          obj_num;          ///< The number of objects per slab

  void            (*ctor)(void *);    ///< The object constructor function
  void            (*dtor)(void *);    ///< The object destructor function

  const char       *name;             ///< Human-readable name for debugging
};

/**
 * Object slab descriptor.
 */
struct KObjectSlab {
  struct ListLink   link;             ///< Link into the containing slab list
  void             *free;             ///< Pointer to the list of free objects
  unsigned          in_use;           ///< The number of objects in use
};

void                 kobject_init(void);

struct KObjectCache *kobject_cache_create(const char  *name,
                                          size_t       size,
                                          void       (*ctor)(void *),
                                          void       (*dtor)(void *));
void                 kobject_cache_destroy(struct KObjectCache *cache);
void                 kobject_cache_reap(void);

void                *kobject_alloc(struct KObjectCache *cache);
void                 kobject_free(struct KObjectCache *cache, void *obj);

#endif  // !__KERNEL_KOBJECT_H__
