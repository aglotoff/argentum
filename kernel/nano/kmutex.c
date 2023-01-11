#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <argentum/armv7/regs.h>
#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/kdebug.h>
#include <argentum/kmutex.h>
#include <argentum/process.h>

/**
 * ----------------------------------------------------------------------------
 * Mutexes
 * ----------------------------------------------------------------------------
 * 
 * Mutex is a sleeping lock, i.e. when a thread tries to acquire a mutex that
 * is locked, it is put to sleep until the mutex becomes available.
 *
 * Mutexes are used if the holding time is long or if the thread needs to sleep
 * while holding the lock.
 *
 */

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
  mutex->thread = NULL;
  mutex->name = name;
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

  // Sleep until the mutex becomes available.
  while (mutex->thread != NULL)
    kthread_sleep(&mutex->queue, KTHREAD_NOT_RUNNABLE);

  mutex->thread = my_thread();

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

  mutex->thread = NULL;
  kthread_wakeup_all(&mutex->queue);

  sched_unlock();
}

/**
 * Check whether the current thread is holding the mutex.
 *
 * @param lock A pointer to the mutex.
 * @return 1 if the current thread is holding the mutex, 0 otherwise.
 */
int
kmutex_holding(struct KMutex *mutex)
{
  struct KThread *thread;

  sched_lock();
  thread = mutex->thread;
  sched_unlock();

  return (thread != NULL) && (thread == my_thread());
}
