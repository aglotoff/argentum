#ifndef __KERNEL_INCLUDE_TICK_H__
#define __KERNEL_INCLUDE_TICK_H__

#include <kernel/core/list.h>

struct KTimeout {
  struct KListLink link;
  unsigned long    remain;
};

unsigned long long k_tick_get(void);
void               k_tick_set(unsigned long long);
void               k_tick(void);

#endif  // !__KERNEL_INCLUDE_TICK_H__
