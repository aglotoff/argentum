#include <kernel/assert.h>
#include <errno.h>

#include <kernel/kmutex.h>
#include <kernel/task.h>

/**
 * Initialize a mutex.
 * 
 * @param lock A pointer to the mutex to be initialized.
 * @param name The name of the mutex (for debugging purposes).
 */
int
kmutex_init(struct KMutex *mutex, const char *name)
{
  list_init(&mutex->queue);
  mutex->owner = NULL;
  mutex->name  = name;

  return 0;
}

/**
 * Acquire the mutex.
 * 
 * @param lock A pointer to the mutex to be acquired.
 */
int
kmutex_lock(struct KMutex *mutex)
{
  struct Task *my_task = task_current();
  int r;

  sched_lock();

  // Sleep until the mutex becomes available.
  while (mutex->owner != NULL) {
    if (mutex->owner == my_task) {
      sched_unlock();
      return -EDEADLK;
    }

    // TODO: priority inheritance

    if ((r = sched_sleep(&mutex->queue, 0, NULL)) != 0) {
      sched_unlock();
      return r;
    }
  }

  mutex->owner = my_task;

  sched_unlock();
  return 0;
}

/**
 * Release the mutex.
 * 
 * @param lock A pointer to the mutex to be released.
 */
int
kmutex_unlock(struct KMutex *mutex)
{
  if (!kmutex_holding(mutex))
    panic("not holding");
  
  sched_lock();

  // TODO: priority inheritance

  mutex->owner = NULL;
  sched_wakeup_one(&mutex->queue, 0);

  sched_unlock();
  return 0;
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
