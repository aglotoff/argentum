#include <kernel/assert.h>
#include <errno.h>

#include <kernel/mutex.h>
#include <kernel/object_pool.h>
#include <kernel/thread.h>
#include <kernel/cprintf.h>

#include "core_private.h"

static struct KObjectPool *mutex_pool;

static void k_mutex_ctor(void *, size_t);
static void k_mutex_dtor(void *, size_t);

void
k_mutex_system_init(void)
{
  if ((mutex_pool = k_object_pool_create("mutex", sizeof(struct KMutex), 0,
                                         k_mutex_ctor, k_mutex_dtor)) == NULL)
    panic("cannot create mutex pool");
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
  mutex->name  = name;
}

void
k_mutex_fini(struct KMutex *mutex)
{
  _k_sched_spin_lock();
  _k_sched_wakeup_all_locked(&mutex->queue, -EINVAL);
  _k_sched_spin_unlock();
}

struct KMutex *
k_mutex_create(const char *name)
{
  struct KMutex *mutex;

  if ((mutex = (struct KMutex *) k_object_pool_get(mutex_pool)) == NULL)
    return NULL;

  mutex->owner = NULL;
  mutex->name  = name;

  return mutex;
}

void
k_mutex_destroy(struct KMutex *mutex)
{
  k_mutex_fini(mutex);
  k_object_pool_put(mutex_pool, mutex);
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

  k_spinlock_acquire(&mutex->lock);

  // Sleep until the mutex becomes available.
  while (mutex->owner != NULL) {
    if (mutex->owner == my_task) {
      k_spinlock_release(&mutex->lock);
      return -EDEADLK;
    }

    _k_sched_spin_lock();

    // Priority inheritance
    if (mutex->owner->priority > my_task->priority)
      _k_sched_set_priority(mutex->owner, my_task->priority);

    r = _k_sched_sleep(&mutex->queue, THREAD_STATE_MUTEX, 0, NULL);

    _k_sched_spin_unlock();

    if (r < 0) {
      k_spinlock_release(&mutex->lock);
      return r;
    }
  }

  mutex->owner = my_task;

  _k_sched_spin_lock();
  mutex->original_priority = my_task->priority;
  _k_sched_spin_unlock();

  k_spinlock_release(&mutex->lock);

  return 0;
}

/**
 * Release the mutex.
 * 
 * @param lock A pointer to the mutex to be released.
 */
int
k_mutex_unlock(struct KMutex *mutex)
{
  struct KThread *my_task = k_thread_current();

  if (!k_mutex_holding(mutex))
    panic("not holding");
  
  k_spinlock_acquire(&mutex->lock);

  _k_sched_spin_lock();

  _k_sched_wakeup_one_locked(&mutex->queue, 0);
  _k_sched_set_priority(my_task, mutex->original_priority);

  _k_sched_spin_unlock();
  
  mutex->owner = NULL;

  k_spinlock_release(&mutex->lock);

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

  k_spinlock_acquire(&mutex->lock);
  owner = mutex->owner;
  k_spinlock_release(&mutex->lock);

  return (owner != NULL) && (owner == k_thread_current());
}

static void
k_mutex_ctor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  k_spinlock_init(&mutex->lock, "k_mutex");
  k_list_init(&mutex->queue);
  mutex->owner = NULL;
}

static void
k_mutex_dtor(void *p, size_t n)
{
  struct KMutex *mutex = (struct KMutex *) p;
  (void) n;

  assert(!k_list_empty(&mutex->queue));
}
