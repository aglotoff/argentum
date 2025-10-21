#include <kernel/core/task.h>
#include <kernel/core/spinlock.h>
#include <kernel/waitqueue.h>

#include "core_private.h"

void
k_waitqueue_init(struct KWaitQueue *chan)
{
  k_list_init(&chan->head);
}

int
k_waitqueue_sleep(struct KWaitQueue *chan, struct KSpinLock *lock)
{
  return _k_sched_sleep(&chan->head, K_TASK_STATE_SLEEP, 0, lock);
}

int
k_waitqueue_timed_sleep(struct KWaitQueue *chan, struct KSpinLock *lock, unsigned long timeout)
{
  return _k_sched_sleep(&chan->head, K_TASK_STATE_SLEEP, timeout, lock);
}

void
k_waitqueue_wakeup_one(struct KWaitQueue *chan)
{
  _k_sched_lock();
  _k_sched_wakeup_one_locked(&chan->head, 0);
  _k_sched_unlock();
}

void
k_waitqueue_wakeup_all(struct KWaitQueue *chan)
{
  _k_sched_lock();
  _k_sched_wakeup_all_locked(&chan->head, 0);
  _k_sched_unlock();
}
