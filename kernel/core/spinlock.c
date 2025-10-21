#include <stddef.h>
#include <string.h>

#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

void
k_spinlock_init(struct KSpinLock *spin, const char *name)
{
  spin->locked = 0;
  spin->cpu    = K_NULL;
  spin->name   = name;
}

void
k_spinlock_acquire(struct KSpinLock *spin)
{
  if (k_spinlock_holding(spin)) {
    k_arch_spinlock_print_callstack(spin);
    k_panic("CPU %x is already holding %s", k_cpu_id(), spin->name);
  }

  // Disable interrupts to avoid deadlocks
  k_irq_state_save();

  k_arch_spinlock_acquire(&spin->locked);

  spin->cpu = _k_cpu();
  k_arch_spinlock_save_callstack(spin);
}

void
k_spinlock_release(struct KSpinLock *spin)
{
  if (!k_spinlock_holding(spin)) {
    k_arch_spinlock_print_callstack(spin);
    k_panic("CPU %d cannot release %s: held by %d\n",
          k_cpu_id(), spin->name, spin->cpu);
  }

  spin->cpu = K_NULL;
  spin->pcs[0] = 0;

  k_arch_spinlock_release(&spin->locked);
  
  k_irq_state_restore();
}

int
k_spinlock_holding(struct KSpinLock *spin)
{
  int r;

  k_irq_state_save();
  r = spin->locked && (spin->cpu == _k_cpu());
  k_irq_state_restore();

  return r;
}
