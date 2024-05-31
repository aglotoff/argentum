#ifndef __CORE_PRIVATE_H
#define __CORE_PRIVATE_H

#include <kernel/spin.h>

struct KThread;
struct ListLink;

void _k_sched_resume(struct KThread *, int);
void _k_sched_may_yield(struct KThread *);
void _k_sched_yield(void);
void _k_sched_enqueue(struct KThread *);
void _k_sched_wakeup_all(struct ListLink *, int);
void _k_sched_wakeup_one(struct ListLink *, int);
int  _k_sched_sleep(struct ListLink *, int, unsigned long, struct KSpinLock *);

extern struct KSpinLock _k_sched_spinlock;

static inline void
_k_sched_lock(void)
{
  k_spinlock_acquire(&_k_sched_spinlock);
}

static inline void
_k_sched_unlock(void)
{
  k_spinlock_release(&_k_sched_spinlock);
}

#endif  // !__CORE_PRIVATE_H

