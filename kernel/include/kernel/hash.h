#ifndef __KERNEL_INCLUDE_KERNEL_HASH_H__
#define __KERNEL_INCLUDE_KERNEL_HASH_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/list.h>
#include <kernel/types.h>

#define HASH_DECLARE(name, n)  struct ListLink name[n]

#define HASH_FOREACH(hash, lp) \
  for (lp = hash; lp < &hash[ARRAY_SIZE(hash)]; lp++)

#define HASH_FOREACH_ENTRY(hash, lp, key) \
  LIST_FOREACH(&hash[key % ARRAY_SIZE(hash)], lp)

#define HASH_INIT(hash)     \
  do {                      \
    struct ListLink *_lp;   \
                            \
    HASH_FOREACH(hash, _lp) \
      list_init(_lp);       \
  } while (0)

#define HASH_PUT(hash, node, key) \
  list_add_back(&hash[key % ARRAY_SIZE(hash)], node);

#define HASH_REMOVE(node)   list_remove(node)

#endif  // !__KERNEL_INCLUDE_KERNEL_HASH_H__
