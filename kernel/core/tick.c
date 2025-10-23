#include <kernel/core/irq.h>
#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>
#include <kernel/core/tick.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/task.h>

#include "core_private.h"

static struct KSpinLock k_tick_lock = K_SPINLOCK_INITIALIZER("k_tick");
static k_tick_t k_tick_counter = 0; // incremented every system tick interrupt
static k_tick_t k_tick_prev_counter = 0; // used for delta tick calculations

/**
 * @brief Tick interrupt handler called periodically by the system timer.
 *
 * This function is responsible for advancing the global system tick
 * and updating kernel subsystems that depend on time progression.
 * It invokes the scheduler’s quantum check routine and adjusts timeouts.
 *
 * This function must be called in interrupt context (typically from
 * the hardware timer ISR).
 */
void
k_tick(void)
{
  k_tick_t delta_tick;

  k_assert(!k_arch_irq_is_enabled());

  _k_sched_check_quantum();

  if (k_cpu_id() != K_CPU_ID_MASTER)
    return;

  k_spinlock_acquire(&k_tick_lock);

  k_tick_counter++;

  delta_tick = k_tick_counter - k_tick_prev_counter;
  k_tick_prev_counter = k_tick_counter;

  k_spinlock_release(&k_tick_lock);

  _k_sched_adjust_timeouts(delta_tick);
  _k_timer_adjust_timeouts(delta_tick);
}

/**
 * @brief Get the current system tick count.
 *
 * @return The current system tick count.
 */
k_tick_t
k_tick_get(void)
{
  k_tick_t counter;

  k_spinlock_acquire(&k_tick_lock);
  counter = k_tick_counter;
  k_spinlock_release(&k_tick_lock);

  return counter;
}

/**
 * @brief Set the system tick counter to a specific value.
 *
 * This function allows manually adjusting the tick counter, typically
 * used during system initialization or time synchronization. It should
 * be used with caution, as modifying the global tick value may affect
 * timeout and timer computations.
 *
 * @param counter The new tick counter value to set.
 */
void
k_tick_set(k_tick_t counter)
{
  k_spinlock_acquire(&k_tick_lock);
  k_tick_counter = counter;
  k_spinlock_release(&k_tick_lock);
}
