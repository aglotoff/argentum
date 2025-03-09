#include <kernel/core/assert.h>
#include <kernel/core/mutex.h>
#include <kernel/core/task.h>

#include "core_private.h"

/**
 * Initialize a statically allocated mutex.
 * 
 * @param lock A pointer to the mutex to be initialized.
 * @param name The name of the mutex (for debugging purposes).
 */
void
k_mutex_init(struct KMutex *mutex, const char *name)
{
  k_list_init(&mutex->queue);
  k_list_null(&mutex->link);
  mutex->type = K_MUTEX_TYPE;
  mutex->owner = NULL;
  mutex->name = name;
  mutex->priority = K_TASK_MAX_PRIORITIES;
  mutex->flags = 0;
}

void
k_mutex_fini(struct KMutex *mutex)
{
  k_assert(mutex != NULL);
  k_assert(mutex->type == K_MUTEX_TYPE);

  _k_sched_lock();

  k_assert(mutex->owner == NULL);

  _k_sched_wakeup_all_locked(&mutex->queue, K_ERR_INVAL);

  _k_sched_unlock();
}

void
_k_mutex_may_raise_priority(struct KMutex *mutex, int priority)
{
  k_assert(mutex->owner != NULL);

  if (mutex->priority > priority) {
    mutex->priority = priority;
    
    // Temporarily raise the owner's priority
    // If the owner is waiting for another mutex, this may lead to
    // priority recalculation for other mutexes and tasks
    if (mutex->owner->priority > priority)
      _k_sched_raise_priority(mutex->owner, priority);
  }
}

static int
k_mutex_try_lock_locked(struct KMutex *mutex)
{
  struct KTask *current = k_task_current();

  // TODO: assert holding sched

  if (mutex->owner != NULL)
    return (mutex->owner == current) ? K_ERR_DEADLK : K_ERR_AGAIN;
  
  // The highest-priority task always locks the mutex first
  k_assert(current->priority <= mutex->priority);

  mutex->owner = current;
  k_list_add_front(&current->owned_mutexes, &mutex->link);
  
  return 0;
}

int
k_mutex_try_lock(struct KMutex *mutex)
{
  int r;

  k_assert(mutex != NULL);
  k_assert(mutex->type == K_MUTEX_TYPE);

  k_assert(k_task_current() != NULL);

  _k_sched_lock();
  r = k_mutex_try_lock_locked(mutex);
  _k_sched_unlock();

  return r;
}

/**
 * Acquire the mutex.
 * 
 * @param lock A pointer to the mutex to be acquired.
 */
int
_k_mutex_timed_lock(struct KMutex *mutex, unsigned long timeout)
{
  struct KTask *my_task = k_task_current();
  int r;

  while ((r = k_mutex_try_lock_locked(mutex)) != 0) {
    if (r != K_ERR_AGAIN)
      break;

    _k_mutex_may_raise_priority(mutex, my_task->priority);

    my_task->sleep_on_mutex = mutex;
    r = _k_sched_sleep(&mutex->queue, K_TASK_STATE_MUTEX, timeout, NULL);
    my_task->sleep_on_mutex = NULL;

    if (r < 0)
      break;
  }

  return r;
}

/**
 * Acquire the mutex.
 * 
 * @param lock A pointer to the mutex to be acquired.
 */
int
k_mutex_timed_lock(struct KMutex *mutex, unsigned long timeout)
{
  struct KTask *my_task = k_task_current();
  int r;

  k_assert(my_task != NULL);

  k_assert(mutex != NULL);
  k_assert(mutex->type == K_MUTEX_TYPE);

  _k_sched_lock();

  r = _k_mutex_timed_lock(mutex, timeout);

  _k_sched_unlock();

  return r;
}

int
_k_mutex_get_highest_priority(struct KListLink *mutex_list)
{
  struct KListLink *link;
  int max_priority;

  max_priority = K_TASK_MAX_PRIORITIES;
  KLIST_FOREACH(mutex_list, link) {
    struct KMutex *mutex = KLIST_CONTAINER(link, struct KMutex, link);

    if (mutex->priority < max_priority)
      max_priority = mutex->priority;
  }

  return max_priority;
}

void
_k_mutex_recalc_priority(struct KMutex *mutex)
{
  if (k_list_is_empty(&mutex->queue)) {
    mutex->priority = K_TASK_MAX_PRIORITIES;
  } else {
    struct KTask *task;

    task = KLIST_CONTAINER(mutex->queue.next, struct KTask, link);
    mutex->priority = task->priority;
  }
}

void
_k_mutex_unlock(struct KMutex *mutex)
{
  k_list_remove(&mutex->link);
  mutex->owner = NULL;
  
  _k_sched_wakeup_one_locked(&mutex->queue, 0);

  _k_mutex_recalc_priority(mutex);
  _k_sched_update_effective_priority();
}

/**
 * Release the mutex.
 * 
 * @param lock A pointer to the mutex to be released.
 */
int
k_mutex_unlock(struct KMutex *mutex)
{
  k_assert(k_mutex_holding(mutex));

  _k_sched_lock();

  _k_mutex_unlock(mutex);

  _k_sched_unlock();

  return 0;
}

/**
 * Check whether the current task is holding the mutex.
 *
 * @param lock A pointer to the mutex.
 * @return 1 if the current task is holding the mutex, 0 otherwise.
 */
int
k_mutex_holding(struct KMutex *mutex)
{
  struct KTask *owner;

  k_assert(mutex != NULL);
  k_assert(mutex->type == K_MUTEX_TYPE);

  _k_sched_lock();
  owner = mutex->owner;
  _k_sched_unlock();

  return (owner != NULL) && (owner == k_task_current());
}
