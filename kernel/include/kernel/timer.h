#ifndef __KERNEL_INCLUDE_KTIMER_H__
#define __KERNEL_INCLUDE_KTIMER_H__

#include <kernel/list.h>

struct KTimer {
  struct KListLink link;
  unsigned long   remain;
  unsigned long   period;
  int             state;
  void          (*callback)(void *);
  void           *callback_arg;
};

enum {
  K_TIMER_STATE_NONE     = 0,
  K_TIMER_STATE_ACTIVE   = 1,
  K_TIMER_STATE_INACTIVE = 2,
};

int  k_timer_create(struct KTimer *, void (*)(void *), void *, unsigned long,
                    unsigned long, int);
int  k_timer_destroy(struct KTimer *);
int  k_timer_start(struct KTimer *);
int  k_timer_stop(struct KTimer *);
void k_timer_tick(void);

#endif  // !__KERNEL_INCLUDE_timer_H__
