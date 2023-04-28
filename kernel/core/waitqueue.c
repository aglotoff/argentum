#include <argentum/kthread.h>
#include <argentum/spinlock.h>
#include <argentum/waitqueue.h>

void
waitqueue_init(struct WaitQueue *wq)
{
  list_init(&wq->head);
}

void
waitqueue_sleep(struct WaitQueue *wq, struct SpinLock *lock)
{
  sched_lock();

  if (lock != NULL)
    spin_unlock(lock);
  
  kthread_sleep(&wq->head, KTHREAD_NOT_RUNNABLE);

  sched_unlock();

  if (lock != NULL)
    spin_lock(lock);
}

void
waitqueue_wakeup_all(struct WaitQueue *wq)
{
  sched_lock();
  kthread_wakeup_all(&wq->head);
  sched_unlock();
}
