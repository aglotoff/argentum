#include <kernel/core/irq.h>
#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>
#include <kernel/core/tick.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/task.h>

#include "core_private.h"

static struct KSpinLock k_tick_lock = K_SPINLOCK_INITIALIZER("k_tick");
static k_tick_t k_tick_counter = 0;

void
k_timer_tick(void)
{
  k_assert(!k_arch_irq_is_enabled());
  k_assert(k_cpu_id() == K_CPU_ID_MASTER);

  _k_sched_timer_tick();
  _k_timer_tick();

  k_spinlock_acquire(&k_tick_lock);
  k_tick_counter++;
  k_spinlock_release(&k_tick_lock);
}


k_tick_t
k_tick_get(void)
{
  k_tick_t counter;

  k_spinlock_acquire(&k_tick_lock);
  counter = k_tick_counter;
  k_spinlock_release(&k_tick_lock);

  return counter;
}

void
k_tick_set(k_tick_t counter)
{
  k_spinlock_acquire(&k_tick_lock);
  k_tick_counter = counter;
  k_spinlock_release(&k_tick_lock);

  // TODO: update timeouts
}
