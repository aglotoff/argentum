#include <kernel/assert.h>
#include <errno.h>

#include <kernel/mutex.h>
#include <kernel/object_pool.h>
#include <kernel/task.h>
#include <kernel/console.h>

#include "core_private.h"

static void k_mutex_ctor(void *, size_t);
static void k_mutex_dtor(void *, size_t);
static void k_mutex_init_common(struct KMutex *, const char *);
static void k_mutex_fini_common(struct KMutex *);

static struct KObjectPool *k_mutex_pool;

void
k_mutex_system_init(void)
{
  k_mutex_pool = k_object_pool_create("k_mutex",
                                      sizeof(struct KMutex),
                                      0,
                                      k_mutex_ctor,
                                      k_mutex_dtor);
  if (k_mutex_pool == NULL)
    panic("cannot create the mutex pool");
}

/**
 * Initialize a statically allocated mutex.
 * 
 * @param lock A pointer to the mutex to be initialized.
 * @param name The name of the mutex (for debugging purposes).
 */
void
k_mutex_init(struct KMutex *mutex, const char *name)
{
  k_mutex_ctor(mutex, sizeof(struct KMutex));
  k_mutex_init_common(mutex, name);
  mutex->flags = K_MUTEX_STATIC;
}

struct KMutex *
k_mutex_create(const char *name)
{
  struct KMutex *mutex;

  if ((mutex = (struct KMutex *) k_object_pool_get(k_mutex_pool)) == NULL)
    return NULL;

  k_mutex_init_common(mutex, name);
  mutex->flags = 0;

  return mutex;
}

static void
k_mutex_init_common(struct KMutex *mutex, const char *name)
{
  mutex->name     = name;
  mutex->priority = K_TASK_MAX_PRIORITIES;
}

void
k_mutex_fini(struct KMutex *mutex)
{
  if ((mutex == NULL) || (mutex->type != K_MUTEX_TYPE))
    panic("bad mutex pointer");
  if (!(mutex->flags & K_MUTEX_STATIC))
    panic("cannot fini non-static mutexes");

  k_mutex_fini_common(mutex);
}

void
k_mutex_destroy(struct KMutex *mutex)
{
  if ((mutex == NULL) || (mutex->type != K_MUTEX_TYPE))
    panic("bad mutex pointer");
  if (mutex->flags & K_MUTEX_STATIC)
    panic("cannot destroy static mutexes");

  k_mutex_fini_common(mutex);

  k_object_pool_put(k_mutex_pool, mutex);
}

static void
k_mutex_fini_common(struct KMutex *mutex)
{
  _k_sched_lock();

  if (mutex->owner != NULL)
    panic("mutex locked");

  _k_sched_wakeup_all_locked(&mutex->queue, -EINVAL);

  _k_sched_unlock();
}

void
_k_mutex_may_raise_priority(struct KMutex *mutex, int priority)
{
  assert(mutex->owner != NULL);

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
    return (mutex->owner == current) ? -EDEADLK : -EAGAIN;
  
  // The highest-priority task always locks the mutex first
  assert(current->priority <= mutex->priority);

  mutex->owner = current;
  k_list_add_front(&current->owned_mutexes, &mutex->link);
  
  return 0;
}

int
k_mutex_try_lock(struct KMutex *mutex)
{
  int r;

  if (k_task_current() == NULL)
    panic("current task is NULL");
  if ((mutex == NULL) || (mutex->type != K_MUTEX_TYPE))
    panic("bad mutex pointer");

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
    if (r != -EAGAIN)
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

  if (my_task == NULL)
    panic("current task is NULL");
  if ((mutex == NULL) || (mutex->type != K_MUTEX_TYPE))
    panic("bad mutex pointer");

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
  if (!k_mutex_holding(mutex))
    panic("not holding");

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

  if ((mutex == NULL) || (mutex->type != K_MUTEX_TYPE))
    panic("bad mutex pointer");

  _k_sched_lock();
  owner = mutex->owner;
  _k_sched_unlock();

  return (owner != NULL) && (owner == k_task_current());
}

static void
k_mutex_ctor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  k_list_init(&mutex->queue);
  k_list_null(&mutex->link);
  mutex->type = K_MUTEX_TYPE;
  mutex->owner = NULL;
}

static void
k_mutex_dtor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  assert(k_list_is_empty(&mutex->queue));
  assert(k_list_is_null(&mutex->link));
  assert(mutex->owner == NULL);
}
