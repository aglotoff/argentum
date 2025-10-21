#ifndef __KERNEL_INCLUDE_TICK_H__
#define __KERNEL_INCLUDE_TICK_H__

#include <kernel/core/types.h>
#include <kernel/core/list.h>

k_tick_t k_tick_get(void);
void k_tick_set(k_tick_t);
void k_sched_tick(void);

#endif  // !__KERNEL_INCLUDE_TICK_H__
