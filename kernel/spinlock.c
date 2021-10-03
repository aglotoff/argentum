#include <assert.h>
#include <stddef.h>

#include "armv7.h"
#include "process.h"
#include "spinlock.h"

void
spin_init(struct Spinlock *lock, const char *name)
{
  lock->locked = 0;
  lock->cpu    = NULL;
  lock->name   = name;
}

/**
 * Acquire the lock.
 * 
 * @param lock The lock to be acquired.
 */
void
spin_lock(struct Spinlock *lock)
{
  int t1, t2;

  // Disable interrupts to avoid deadlock.
  irq_save();

  if (spin_holding(lock))
    panic("already holding %s", lock->name);

  __asm__ __volatile__(
    "\t1:\n"
    "\tldrex   %1, [%0]\n"      // Read the lock field
    "\tcmp     %1, #0\n"        // Compare with 0
    "\twfene\n"                 // Not 0 means already locked, do WFE
    "\tbne     1b\n"            // Retry after woken up by event
    "\tmov     %1, #1\n" 
    "\tstrex   %2, %1, [%0]\n"  // Try to store 1 into the lock field
    "\tcmp     %2, #0\n"        // Check return value: 0=OK, 1=failed
    "\tbne     1b\n"            // If store failed, try again
    "\tdmb\n"                   // Memory barrier BEFORE accessing the resource
    : "+r"(lock), "=r"(t1), "=r"(t2)
    :
    : "memory", "cc");

  // Record info about lock acquisition for debugging.
  lock->cpu = mycpu();
}

/**
 * Release the lock.
 * 
 * @param lock The lock to be released.
 */
void
spin_unlock(struct Spinlock *lock)
{
  int t;

  lock->cpu = NULL;

  __asm__ __volatile__(
    "\tmov     %1, #0\n"
    "\tdmb\n"                   // Memory barier BEFORE releasing the resource
    "\tstr     %1, [%0]\n"      // Write 0 into the lock field
    "\tdsb\n"                   // Ensure update has completed before SEV
    "\tsev\n"                   // Send event to wakeup other CPUS in WFE mode
    : "+r"(lock), "=r"(t)
    :
    : "cc", "memory"
  );
  
  irq_restore();
}

/**
 * Check whether this CPU is holding the lock.
 *
 * @param lock The spinlock.
 * @return 1 if this CPU is holding the lock, 0 otherwise.
 */
int
spin_holding(struct Spinlock *lock)
{
  int r;

  irq_save();
  r = lock->locked && (lock->cpu == mycpu());
  irq_restore();

  return r;
}

/**
 * Save the current CPU interrupt state and disable interrupts.
 */
void
irq_save(void)
{
  uint32_t psr;

  psr = read_cpsr();
  write_cpsr(psr | PSR_I | PSR_F);

  if (mycpu()->irq_lock++ == 0)
    mycpu()->irq_flags = ~psr & (PSR_I | PSR_F);
}

/**
 * Restore the previous interrupt state.
 * 
 * @param flags The value obtained from a previous call to irq_save().
 */
void
irq_restore(void)
{
  uint32_t psr;

  psr = read_cpsr();
  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  if (--mycpu()->irq_lock < 0)
    panic("interruptible");

  if (mycpu()->irq_lock == 0)
    write_cpsr(psr & ~mycpu()->irq_flags);
}
