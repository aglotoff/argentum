#include <assert.h>

#include <argentum/kmutex.h>
#include <argentum/task.h>

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
  spin_init(&mutex->lock, name);
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
  spin_lock(&mutex->lock);

  // TODO: priority inheritance

  // Sleep until the mutex becomes available.
  while (mutex->owner != NULL)
    task_sleep(&mutex->queue, TASK_STATE_SLEEPING_MUTEX, &mutex->lock);

  mutex->owner = task_current();

  spin_unlock(&mutex->lock);
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
  
  spin_lock(&mutex->lock);

  // TODO: priority inheritance

  mutex->owner = NULL;
  task_wakeup_all(&mutex->queue);

  spin_unlock(&mutex->lock);
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

  spin_lock(&mutex->lock);
  owner = mutex->owner;
  spin_unlock(&mutex->lock);

  return (owner != NULL) && (owner == task_current());
}
