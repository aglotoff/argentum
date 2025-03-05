#include <kernel/core/assert.h>
#include <kernel/core/irq.h>
#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>

#include "core_private.h"

/**
 * Save the current CPU interrupt state and disable interrupts.
 */
void
k_irq_state_save(void)
{
  int status = k_arch_irq_state_save();

  if (_k_cpu()->irq_save_count++ == 0)
    _k_cpu()->irq_flags = status;
}

/**
 * Restore the interrupt state saved by a preceding k_irq_state_save() call.
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
 * Notify the kernel that an IRQ handler has started.
 */
void
k_irq_handler_begin(void)
{
  k_irq_state_save();
  ++_k_cpu()->lock_count;
  k_irq_state_restore();
}

/**
 * Notify the kernel that an IRQ handler has finished.
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
    struct KTask *task = cpu->task;

    // Before resuming the current task, check whether it must give up the CPU
    // or exit.
    if ((task != NULL) && (task->flags & K_TASK_FLAG_RESCHEDULE)) {
      task->flags &= ~K_TASK_FLAG_RESCHEDULE;

      _k_sched_enqueue(task);
      _k_sched_yield_locked();
    }
  }

  _k_sched_unlock();
}
