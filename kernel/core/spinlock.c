#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

static void
k_spinlock_print_callstack(struct KSpinLock *spin)
{
#ifdef K_ON_SPINLOCK_DEBUG_PC
  int i;

  for (i = 0; i < K_SPINLOCK_MAX_PCS && spin->pcs[i]; i++) {
    K_ON_SPINLOCK_DEBUG_PC(spin->pcs[i]);
  }
#else
  (void) spin;
#endif
}

/**
 * @brief Initialize a spinlock.
 *
 * @param spin Pointer to the spinlock to initialize.
 * @param name Human-readable name for the spinlock, used in diagnostics.
 *
 * @note The spinlock must be initialized before any call to
 *       `k_spinlock_acquire()` or `k_spinlock_release()`.
 */
void
k_spinlock_init(struct KSpinLock *spin, const char *name)
{
  spin->locked = 0;
  spin->cpu = K_NULL;
  spin->name = name;
}

/**
 * @brief Acquire a spinlock.
 *
 * Spins until the lock becomes available, then marks it as held by the
 * current CPU. Interrupts are disabled while the lock is held to prevent
 * deadlocks and ensure atomic access to protected data.
 *
 * @param spin Pointer to the spinlock to acquire.
 */
void
k_spinlock_acquire(struct KSpinLock *spin)
{
#ifndef K_NDEBUG
  if (k_spinlock_holding(spin)) {
    k_spinlock_print_callstack(spin);
    k_panic("CPU %x is already holding %s", k_cpu_id(), spin->name);
  }
#endif

  k_irq_state_save();

  k_arch_spinlock_acquire(&spin->locked);

  spin->cpu = _k_cpu();
  k_arch_spinlock_save_callstack(spin);
}

/**
 * @brief Release a spinlock.
 *
 * Releases the lock held by the current CPU and restores the previous
 * interrupt state.
 *
 * @param spin Pointer to the spinlock to release.
 *
 * @note The caller must ensure that the critical section protected by the
 *       lock is complete before releasing it.
 */
void
k_spinlock_release(struct KSpinLock *spin)
{
#ifndef K_NDEBUG
  if (!k_spinlock_holding(spin)) {
    k_spinlock_print_callstack(spin);
    k_panic("CPU %d cannot release %s: held by %d\n",
            k_cpu_id(), spin->name, spin->cpu);
  }
#endif

  spin->cpu = K_NULL;
  spin->pcs[0] = 0;

  k_arch_spinlock_release(&spin->locked);
  
  k_irq_state_restore();
}

/**
 * @brief Test whether the current CPU holds a spinlock.
 *
 * Checks if the specified spinlock is currently locked and owned by the
 * calling CPU.
 *
 * @param spin Pointer to the spinlock to test.
 *
 * @return Nonzero (true) if the current CPU holds the lock; zero (false) otherwise.
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
