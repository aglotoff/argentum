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

// unsigned
// spin_lock(struct Spinlock *lock)
// {

// }

// void
// spin_unlock(struct Spinlock *lock)
// {

// }

/**
 * Save the current CPU interrupt state and disable interrupts.
 * 
 * @return Implementation-defined value representing the interrupt state prior
 * to the call.
 */
int
irq_save(void)
{
  uint32_t psr;

  psr = read_cpsr();
  write_cpsr(psr | PSR_I | PSR_F);
  return ~psr & (PSR_I | PSR_F);
}

/**
 * Restore the previous interrupt state.
 * 
 * @param flags The value obtained from a previous call to irq_save().
 */
void
irq_restore(int flags)
{
  uint32_t psr;

  psr = read_cpsr();
  write_cpsr(psr & ~flags);
}
