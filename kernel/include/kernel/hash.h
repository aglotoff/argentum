#ifndef __KERNEL_INCLUDE_KERNEL_HASH_H__
#define __KERNEL_INCLUDE_KERNEL_HASH_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/core/list.h>
#include <kernel/types.h>

#define HASH_DECLARE(name, n)  struct KListLink name[n]

#define HASH_FOREACH(hash, lp) \
  for (lp = hash; lp < &hash[ARRAY_SIZE(hash)]; lp++)

#define HASH_FOREACH_ENTRY(hash, lp, key) \
  KLIST_FOREACH(&hash[key % ARRAY_SIZE(hash)], lp)

#define HASH_INIT(hash)     \
  do {                      \
    struct KListLink *_lp;   \
                            \
    HASH_FOREACH(hash, _lp) \
      k_list_init(_lp);       \
  } while (0)

#define HASH_PUT(hash, node, key) \
  k_list_add_back(&hash[key % ARRAY_SIZE(hash)], node);

#define HASH_REMOVE(node)   k_list_remove(node)

#endif  // !__KERNEL_INCLUDE_KERNEL_HASH_H__
