#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <armv7/regs.h>
#include <cprintf.h>
#include <cpu.h>
#include <kdebug.h>
#include <process.h>

#include <sync.h>

/**
 * ----------------------------------------------------------------------------
 * Spinlocks
 * ----------------------------------------------------------------------------
 * 
 * Spinlocks provide mutual exclusion, ensuring only one CPU at a time can hold
 * the lock. A thread trying to acquire the lock waits in a loop repeatedly
 * testing the lock until it becomes available.
 *
 * Spinlocks are used if the holding time is short or if the data to be
 * protected is accessed from an interrupt handler context.
 *
 */

static void spin_save_caller_pcs(struct SpinLock *);
static void spin_print_caller_pcs(struct SpinLock *);

/**
 * Initialize a spinlock.
 * 
 * @param lock A pointer to the spinlock to be initialized.
 * @param name The name of the spinlock (for debugging purposes).
 */
void
spin_init(struct SpinLock *lock, const char *name)
{
  lock->locked = 0;
  lock->cpu    = NULL;
  lock->name   = name;
}

/**
 * @brief Acquire the spinlock.
 *
 * @param lock A pointer to the spinlock to be acquired.
 */
void
spin_lock(struct SpinLock *lock)
{
  int t1, t2;

  // Disable interrupts to avoid deadlock.
  irq_save();

  if (spin_holding(lock)) {
    spin_print_caller_pcs(lock);
    panic("CPU %d is already holding %s", cpu_id(), lock->name);
  }

  asm volatile(
    "\tmov     %2, #1\n"        // Load the "lock acquired" value
    "\t1:\n"
    "\tldrex   %1, [%0]\n"      // Read the lock field
    "\tcmp     %1, #0\n"        // Is the lock free?
    "\tbne     1b\n"            // No - try again
    "\tstrex   %1, %2, [%0]\n"  // Try and acquire the lock
    "\tcmp     %1, #0\n"        // Did this succeed?
    "\tbne     1b\n"            // No - try again
    : "+r"(lock), "=r"(t1), "=r"(t2)
    :
    : "memory", "cc");

  // Record information about lock acquisition for debugging purposes.
  lock->cpu = my_cpu();
  spin_save_caller_pcs(lock);
}

/**
 * Release the spinlock.
 * 
 * @param lock A pointer to the spinlock to be released.
 */
void
spin_unlock(struct SpinLock *lock)
{
  int t;

  if (!spin_holding(lock)) {
    spin_print_caller_pcs(lock);
    panic("CPU %d cannot release %s: held by %d\n",
          cpu_id(), lock->name, lock->cpu);
  }

  lock->cpu = NULL;
  lock->pcs[0] = 0;

  asm volatile(
    "\tmov     %1, #0\n"
    "\tstr     %1, [%0]\n"
    : "+r"(lock), "=r"(t)
    :
    : "cc", "memory"
  );
  
  irq_restore();
}

/**
 * Check whether the current CPU is holding the lock.
 *
 * @param lock A pointer to the spinlock.
 * @return 1 if the current CPU is holding the lock, 0 otherwise.
 */
int
spin_holding(struct SpinLock *lock)
{
  int r;

  irq_save();
  r = lock->locked && (lock->cpu == my_cpu());
  irq_restore();

  return r;
}

// Record the current stack backtrace by following the frame pointer chain.
static void
spin_save_caller_pcs(struct SpinLock *lock)
{
  uint32_t *fp;
  int i;

  fp = (uint32_t *) r11_get();

  for (i = 0; (fp != NULL) && (i < NCALLERPCS); i++) {
    lock->pcs[i] = fp[-1];
    fp = (uint32_t *) fp[-3];
  }

  for ( ; i < NCALLERPCS; i++)
    lock->pcs[i] = 0;
}

static void
spin_print_caller_pcs(struct SpinLock *lock)
{
  struct PcDebugInfo info;
  uintptr_t pcs[NCALLERPCS];
  int i;

  for (i = 0; i < NCALLERPCS; i++)
    pcs[i] = lock->pcs[i];

  for (i = 0; i < NCALLERPCS && pcs[i]; i++) {
    debug_info_pc(pcs[i], &info);
    cprintf("  [%p] %s (%s at line %d)\n",
            pcs[i],
            info.fn_name, info.file, info.line);
  }
}

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
