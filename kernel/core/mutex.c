#include <kernel/assert.h>
#include <errno.h>

#include <kernel/mutex.h>
#include <kernel/object_pool.h>
#include <kernel/thread.h>
#include <kernel/cprintf.h>

#include "core_private.h"

static void k_mutex_ctor(void *, size_t);
static void k_mutex_dtor(void *, size_t);
static void k_mutex_init_common(struct KMutex *, const char *);

static struct KObjectPool *k_mutex_pool;

void
k_mutex_system_init(void)
{
  if ((k_mutex_pool = k_object_pool_create("mutex",
                                           sizeof(struct KMutex),
                                           0,
                                           k_mutex_ctor,
                                           k_mutex_dtor)) == NULL)
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
}

void
k_mutex_fini(struct KMutex *mutex)
{
  _k_sched_lock();
  _k_sched_wakeup_all_locked(&mutex->queue, -EINVAL);
  _k_sched_unlock();
}

struct KMutex *
k_mutex_create(const char *name)
{
  struct KMutex *mutex;

  if ((mutex = (struct KMutex *) k_object_pool_get(k_mutex_pool)) == NULL)
    return NULL;

  k_mutex_init_common(mutex, name);

  return mutex;
}

void
k_mutex_destroy(struct KMutex *mutex)
{
  k_mutex_fini(mutex);
  k_object_pool_put(k_mutex_pool, mutex);
}

/**
 * Acquire the mutex.
 * 
 * @param lock A pointer to the mutex to be acquired.
 */
int
k_mutex_lock(struct KMutex *mutex)
{
  struct KThread *my_task = k_thread_current();
  int r;

  _k_sched_lock();

  // Sleep until the mutex becomes available
  while (mutex->owner != NULL) {
    if (mutex->owner == my_task) {
      _k_sched_unlock();
      return -EDEADLK;
    }

    if (mutex->priority > my_task->priority) {
      mutex->priority = my_task->priority;
      
      // Temporarily raise the owner's priority
      if (my_task->priority < mutex->owner->priority)
        _k_sched_set_priority(mutex->owner, my_task->priority);
    }

    my_task->wait_mutex = mutex;
    r = _k_sched_sleep(&mutex->queue, THREAD_STATE_MUTEX, 0, NULL);

    if (r < 0) {
      _k_sched_unlock();
      return r;
    }
  }

  mutex->owner = my_task;

  // The highest-priority thread always locks the mutex first
  assert(my_task->priority < mutex->priority);

  k_list_remove(&mutex->link);
  k_list_add_front(&my_task->mutex_list, &mutex->link);

  _k_sched_unlock();

  return 0;
}

void
_k_sched_calc_priority(struct KThread *thread)
{
  struct KListLink *l;
  int priority;
  
  priority = thread->saved_priority;
  KLIST_FOREACH(&thread->mutex_list, l) {
    struct KMutex *mutex = KLIST_CONTAINER(l, struct KMutex, link);

    if (mutex->priority < priority)
      priority = mutex->priority;
  }

  assert(priority >= thread->priority);

  if (priority > thread->priority)
    _k_sched_set_priority(thread, priority);
}

void
_k_mutex_recalc_priority(struct KMutex *mutex)
{
  if (k_list_empty(&mutex->queue)) {
    mutex->priority = THREAD_MAX_PRIORITIES;
  } else {
    struct KThread *thread;

    thread = KLIST_CONTAINER(mutex->queue.next, struct KThread, link);
    mutex->priority = thread->priority;
  }
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

  k_list_remove(&mutex->link);

  _k_sched_wakeup_one_locked(&mutex->queue, 0);
  _k_mutex_recalc_priority(mutex);

  _k_sched_calc_priority(k_thread_current());

  mutex->owner = NULL;

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
  struct KThread *owner;

  _k_sched_lock();
  owner = mutex->owner;
  _k_sched_unlock();

  return (owner != NULL) && (owner == k_thread_current());
}

static void
k_mutex_ctor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  k_list_init(&mutex->queue);
}

static void
k_mutex_dtor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  assert(!k_list_empty(&mutex->queue));
}

static void
k_mutex_init_common(struct KMutex *mutex, const char *name)
{
  mutex->owner    = NULL;
  mutex->name     = name;
  mutex->priority = THREAD_MAX_PRIORITIES;
}
