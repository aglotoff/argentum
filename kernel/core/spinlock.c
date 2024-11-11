#include <stddef.h>
#include <string.h>

#include <kernel/assert.h>
#include <kernel/console.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/kdebug.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>

#include "core_private.h"

/**
 * Initialize a spinlock.
 * 
 * @param lock A pointer to the spinlock to be initialized.
 * @param name The name of the spinlock (for debugging purposes).
 */
void
k_spinlock_init(struct KSpinLock *spin, const char *name)
{
  spin->locked = 0;
  spin->cpu    = NULL;
  spin->name   = name;
}

/**
 * Acquire the spinlock.
 *
 * @param lock A pointer to the spinlock to be acquired.
 */
void
k_spinlock_acquire(struct KSpinLock *spin)
{
  if (k_spinlock_holding(spin)) {
    k_arch_spinlock_print_callstack(spin);
    panic("CPU %x is already holding %s", k_cpu_id(), spin->name);
  }

  // Disable interrupts to avoid deadlocks
  k_irq_state_save();

  k_arch_spinlock_acquire(&spin->locked);

  spin->cpu = _k_cpu();
  k_arch_spinlock_save_callstack(spin);
}

/**
 * Release the spinlock.
 * 
 * @param lock A pointer to the spinlock to be released.
 */
void
k_spinlock_release(struct KSpinLock *spin)
{
  if (!k_spinlock_holding(spin)) {
    k_arch_spinlock_print_callstack(spin);
    panic("CPU %d cannot release %s: held by %d\n",
          k_cpu_id(), spin->name, spin->cpu);
  }

  spin->cpu = NULL;
  spin->pcs[0] = 0;

  k_arch_spinlock_release(&spin->locked);
  
  k_irq_state_restore();
}

/**
 * Check whether the current CPU is holding the lock.
 *
 * @param lock A pointer to the spinlock.
 * @return 1 if the current CPU is holding the lock, 0 otherwise.
 */
int
k_spinlock_holding(struct KSpinLock *spin)
{
  int r;

  k_irq_state_save();
  r = spin->locked && (spin->cpu == _k_cpu());
  k_irq_state_restore();

  return r;
}
