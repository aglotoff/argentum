#ifndef __INCLUDE_KERNEL_CORE_TIMER_H__
#define __INCLUDE_KERNEL_CORE_TIMER_H__

#include <kernel/core/types.h>

/**
 * @brief Kernel software timer descriptor.
 *
 * Represents a timer managed by the kernel scheduler or timeout subsystem.
 * Each timer can be configured for one-shot or periodic operation.
 */
struct KTimer {
  int type;
  struct KTimeoutEntry queue_entry;
  int state;
  void (*callback)(void *);
  void *callback_arg;
  k_tick_t delay;
  k_tick_t period;
};

int k_timer_create(struct KTimer *, void (*)(void *), void *, k_tick_t, k_tick_t);
int k_timer_destroy(struct KTimer *);
int k_timer_start(struct KTimer *);
int k_timer_stop(struct KTimer *);

#endif  // !__INCLUDE_KERNEL_CORE_TIMER_H__
