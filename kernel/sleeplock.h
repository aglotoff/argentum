#ifndef __KERNEL_SLEEPLOCK_H__
#define __KERNEL_SLEEPLOCK_H__

#include "process.h"
#include "spinlock.h"

struct Sleeplock {
  struct Process   *process;
  struct Spinlock   lock;
  struct WaitQueue  wait_queue;
  const char       *name;
};

void sleep_init(struct Sleeplock *, const char *);
void sleep_lock(struct Sleeplock *);
void sleep_unlock(struct Sleeplock *);
int  sleep_holding(struct Sleeplock *);

#endif  // !__KERNEL_SLEEPLOCK_H__
