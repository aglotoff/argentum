#include <kernel/core/assert.h>
#include <kernel/core/irq.h>
#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>

#include "core_private.h"

/**
 * @brief Save and disable the current interrupt state.
 *
 * This function captures the CPU's current interrupt enable state and, if this
 * is the first nested call, disables interrupts globally. Nested calls simply
 * increment a per-CPU reference counter, allowing multiple critical sections
 * to coexist safely.
 *
 * @note Must be paired with a corresponding call to `k_irq_state_restore()`.
 */
void
k_irq_state_save(void)
{
  int flags = k_arch_irq_state_save();

  if (_k_cpu()->irq_save_count++ == 0)
    _k_cpu()->irq_flags = flags;
}

/**
 * @brief Restore the CPU interrupt state after a critical section.
 *
 * Decrements the per-CPU interrupt disable nesting counter. When the counter
 * reaches zero, the saved interrupt state is restored, potentially re-enabling
 * interrupts if they were previously active.
 */
void
k_irq_state_restore(void)
{
  struct KCpu *cpu;
  int count;

  k_assert(!k_arch_irq_is_enabled());

  cpu = _k_cpu();
  count = --cpu->irq_save_count;

  k_assert(count >= 0);

  if (count == 0)
    k_arch_irq_state_restore(cpu->irq_flags);
}

/**
 * @brief Mark the beginning of an interrupt handler.
 *
 * This function is called at the entry point of an interrupt service routine
 * (ISR). Increments the per-CPU internal lock counter. This ensures that
 * nested interrupts and re-entrant handler logic remain consistent with the
 * kernel’s locking model.
 */
void
k_irq_handler_begin(void)
{
  k_irq_state_save();
  ++_k_cpu()->lock_count;
  k_irq_state_restore();
}

/**
 * @brief Mark the end of an interrupt handler.
 *
 * Called just before exiting an ISR. This function decrements the CPU’s
 * internal lock counter and checks if a reschedule is requested.
 *
 * @note Must be called once for every `k_irq_handler_begin()` invocation.
 * @warning This function may trigger a context switch before returning.
 */
void
k_irq_handler_end(void)
{
  struct KCpu *cpu;
  int count;

  _k_sched_lock();

  cpu = _k_cpu();
  count = --cpu->lock_count;

  k_assert(count >= 0);

  if (count == 0) {
    struct KTask *current = cpu->task;

    if ((current != K_NULL) && (current->flags & K_TASK_FLAG_RESCHEDULE)) {
      current->flags &= ~K_TASK_FLAG_RESCHEDULE;
      _k_sched_enqueue(current);
      _k_sched_yield_locked();
    }
  }

  _k_sched_unlock();
}
