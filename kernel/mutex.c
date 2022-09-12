#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <armv7/regs.h>
#include <cprintf.h>
#include <cpu.h>
#include <kdebug.h>
#include <process.h>

#include <mutex.h>

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
mutex_init(struct Mutex *mutex, const char *name)
{
  spin_init(&mutex->lock, name);
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
mutex_lock(struct Mutex *mutex)
{
  spin_lock(&mutex->lock);

  // Sleep until the mutex becomes available.
  while (mutex->thread != NULL)
    kthread_sleep(&mutex->queue, &mutex->lock);

  mutex->thread = my_thread();

  spin_unlock(&mutex->lock);
}

/**
 * Release the mutex.
 * 
 * @param lock A pointer to the mutex to be released.
 */
void
mutex_unlock(struct Mutex *mutex)
{
  if (!mutex_holding(mutex))
    panic("not holding");
  
  spin_lock(&mutex->lock);

  mutex->thread = NULL;
  kthread_wakeup(&mutex->queue);

  spin_unlock(&mutex->lock);
}

/**
 * Check whether the current thread is holding the mutex.
 *
 * @param lock A pointer to the mutex.
 * @return 1 if the current thread is holding the mutex, 0 otherwise.
 */
int
mutex_holding(struct Mutex *mutex)
{
  struct KThread *thread;

  spin_lock(&mutex->lock);
  thread = mutex->thread;
  spin_unlock(&mutex->lock);

  return (thread != NULL) && (thread == my_thread());
}
