#ifndef __KERNEL_INCLUDE_KERNEL_TIMER_H__
#define __KERNEL_INCLUDE_KERNEL_TIMER_H__

#include <kernel/core/tick.h>

struct KTimer {
  struct KTimeout entry;
  int                state;
  void             (*callback)(void *);
  void              *callback_arg;
  unsigned long      delay;
  unsigned long      period;
};

enum {
  K_TIMER_STATE_NONE     = 0,
  K_TIMER_STATE_ACTIVE   = 1,
  K_TIMER_STATE_INACTIVE = 2,
  K_TIMER_STATE_RUNNING  = 3,
};

int  k_timer_init(struct KTimer *, void (*)(void *), void *, unsigned long,
                    unsigned long, int);
int  k_timer_fini(struct KTimer *);
int  k_timer_start(struct KTimer *);
int  k_timer_stop(struct KTimer *);

void k_tick(void);

#endif  // !__KERNEL_INCLUDE_KERNEL_TIMER_H__
