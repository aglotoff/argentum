#include <string.h>

#include <smp.h>
#include <irq.h>
#include <kernel.h>
#include <spinlock.h>

static void spin_pcs_print(struct SpinLock *);

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
    spin_pcs_print(spin);
    panic("CPU %x is already holding %s", smp_id(), spin->name);
  }

  // Disable interrupts to avoid deadlocks
  irq_save();

  arch_spin_lock(&spin->locked);

  // Record information about the caller
  spin->cpu = smp_cpu();
  arch_spin_pcs_save(spin->pcs, SPIN_MAX_PCS);
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
    spin_pcs_print(spin);
    panic("Cannot release %s: held by %d\n", smp_id(), spin->name, spin->cpu);
  }

  spin->cpu = NULL;
  spin->pcs[0] = 0;

  arch_spin_unlock(&spin->locked);
  
  irq_restore();
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

  irq_save();
  r = spin->locked && (spin->cpu == smp_cpu());
  irq_restore();

  return r;
}

// Display the recorded call stack along with debugging information
static void
spin_pcs_print(struct SpinLock *spin)
{
  int i;

  for (i = 0; i < SPIN_MAX_PCS && spin->pcs[i]; i++)
    kprintf("  [%p] \n", spin->pcs[i]);
}
