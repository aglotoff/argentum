#include <kernel/thread.h>
#include <kernel/spin.h>
#include <kernel/wchan.h>

/**
 * Initialize the wait channel.
 * 
 * @param chan A pointer to the channel to be initialized.
 */
void
wchan_init(struct WaitChannel *chan)
{
  list_init(&chan->head);
}

/**
 * Wait for the resource associated with the given wait channel to become
 * available and release an optional spinlock.
 * 
 * @param chan A pointer to the wait channel to sleep on.
 * @param lock A pointer to the spinlock to be released.
 */
int
wchan_sleep(struct WaitChannel *chan, struct SpinLock *lock)
{
  return sched_sleep(&chan->head, 0, lock);
}

/**
 * Wakeup the highest-priority task sleeling on the wait channel.
 * 
 * @param chan A pointer to the wait channel
 */
void
wchan_wakeup_one(struct WaitChannel *chan)
{
  sched_lock();
  sched_wakeup_one(&chan->head, 0);
  sched_unlock();
}

/**
 * Wakeup all tasks sleeling on the wait channel.
 * 
 * @param chan A pointer to the wait channel
 */
void
wchan_wakeup_all(struct WaitChannel *chan)
{
  sched_lock();
  sched_wakeup_all(&chan->head, 0);
  sched_unlock();
}
