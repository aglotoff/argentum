#include <kernel/core/task.h>
#include <kernel/core/spinlock.h>
#include <kernel/waitqueue.h>

#include "core_private.h"

/**
 * Initialize the wait connection.
 * 
 * @param chan A pointer to the connection to be initialized.
 */
void
k_waitqueue_init(struct KWaitQueue *chan)
{
  k_list_init(&chan->head);
}

/**
 * Wait for the resource associated with the given wait connection to become
 * available and release an optional spinlock.
 * 
 * @param chan A pointer to the wait connection to sleep on.
 * @param lock A pointer to the spinlock to be released.
 */
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

/**
 * Wakeup the highest-priority task sleeling on the wait connection.
 * 
 * @param chan A pointer to the wait connection
 */
void
k_waitqueue_wakeup_one(struct KWaitQueue *chan)
{
  _k_sched_lock();
  _k_sched_wakeup_one_locked(&chan->head, 0);
  _k_sched_unlock();
}

/**
 * Wakeup all tasks sleeling on the wait connection.
 * 
 * @param chan A pointer to the wait connection
 */
void
k_waitqueue_wakeup_all(struct KWaitQueue *chan)
{
  _k_sched_lock();
  _k_sched_wakeup_all_locked(&chan->head, 0);
  _k_sched_unlock();
}
