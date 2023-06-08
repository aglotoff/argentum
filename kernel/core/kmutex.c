#include <assert.h>

#include <kernel/kmutex.h>
#include <kernel/task.h>

/**
 * Initialize a mutex.
 * 
 * @param lock A pointer to the mutex to be initialized.
 * @param name The name of the mutex (for debugging purposes).
 */
void
kmutex_init(struct KMutex *mutex, const char *name)
{
  list_init(&mutex->queue);
  mutex->owner = NULL;
  mutex->name  = name;
}

/**
 * Acquire the mutex.
 * 
 * @param lock A pointer to the mutex to be acquired.
 */
void
kmutex_lock(struct KMutex *mutex)
{
  sched_lock();

  // TODO: priority inheritance

  // Sleep until the mutex becomes available.
  while (mutex->owner != NULL)
    task_sleep(&mutex->queue, TASK_STATE_MUTEX, &__sched_lock);

  mutex->owner = task_current();

  sched_unlock();
}

/**
 * Release the mutex.
 * 
 * @param lock A pointer to the mutex to be released.
 */
void
kmutex_unlock(struct KMutex *mutex)
{
  if (!kmutex_holding(mutex))
    panic("not holding");
  
  sched_lock();

  // TODO: priority inheritance

  mutex->owner = NULL;
  sched_wakeup_all(&mutex->queue);

  sched_unlock();
}

/**
 * Check whether the current task is holding the mutex.
 *
 * @param lock A pointer to the mutex.
 * @return 1 if the current task is holding the mutex, 0 otherwise.
 */
int
kmutex_holding(struct KMutex *mutex)
{
  struct Task *owner;

  sched_lock();
  owner = mutex->owner;
  sched_unlock();

  return (owner != NULL) && (owner == task_current());
}
