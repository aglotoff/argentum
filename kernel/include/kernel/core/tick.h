#ifndef __INCLUDE_KERNEL_CORE_TICK_H__
#define __INCLUDE_KERNEL_CORE_TICK_H__

#include <kernel/core/types.h>

k_tick_t k_tick_get(void);
void k_tick_set(k_tick_t);
void k_tick(void);

#endif  // !__INCLUDE_KERNEL_CORE_TICK_H__
