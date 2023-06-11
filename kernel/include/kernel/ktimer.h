#ifndef __KERNEL_INCLUDE_KTIMER_H__
#define __KERNEL_INCLUDE_KTIMER_H__

#include <kernel/list.h>

struct KTimer {
  struct ListLink link;
  unsigned long   remain;
  unsigned long   period;
  int             state;
  void          (*callback)(void *);
  void           *callback_arg;
};

enum {
  KTIMER_STATE_NONE     = 0,
  KTIMER_STATE_ACTIVE   = 1,
  KTIMER_STATE_INACTIVE = 2,
};

int  ktimer_create(struct KTimer *, void (*)(void *), void *, unsigned long,
                  unsigned long, int);
int  ktimer_destroy(struct KTimer *);
int  ktimer_start(struct KTimer *);
int  ktimer_stop(struct KTimer *);
void ktimer_tick(void);

#endif  // !__KERNEL_INCLUDE_KTIMER_H__
