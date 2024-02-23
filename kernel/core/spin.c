#include <stddef.h>
#include <string.h>

#include <kernel/assert.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/kdebug.h>
#include <kernel/process.h>
#include <kernel/spin.h>

/**
 * Initialize a spinlock.
 * 
 * @param lock A pointer to the spinlock to be initialized.
 * @param name The name of the spinlock (for debugging purposes).
 */
void
spin_init(struct SpinLock *spin, const char *name)
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
spin_lock(struct SpinLock *spin)
{
  if (spin_holding(spin)) {
    spin_arch_pcs_print(spin);
    panic("CPU %x is already holding %s", cpu_id(), spin->name);
  }

  // Disable interrupts to avoid deadlocks
  cpu_irq_save();

  spin_arch_lock(&spin->locked);

  spin->cpu = cpu_current();
  spin_arch_pcs_save(spin);
}

/**
 * Release the spinlock.
 * 
 * @param lock A pointer to the spinlock to be released.
 */
void
spin_unlock(struct SpinLock *spin)
{
  if (!spin_holding(spin)) {
    spin_arch_pcs_print(spin);
    panic("CPU %d cannot release %s: held by %d\n",
          cpu_id(), spin->name, spin->cpu);
  }

  spin->cpu = NULL;
  spin->pcs[0] = 0;

  spin_arch_unlock(&spin->locked);
  
  cpu_irq_restore();
}

/**
 * Check whether the current CPU is holding the lock.
 *
 * @param lock A pointer to the spinlock.
 * @return 1 if the current CPU is holding the lock, 0 otherwise.
 */
int
spin_holding(struct SpinLock *spin)
{
  int r;

  cpu_irq_save();
  r = spin->locked && (spin->cpu == cpu_current());
  cpu_irq_restore();

  return r;
}
