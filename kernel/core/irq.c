#include <kernel/assert.h>
#include <errno.h>
#include <stddef.h>

#include <kernel/process.h>
#include <kernel/core/irq.h>
#include <kernel/console.h>
#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>

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
  if (k_arch_irq_is_enabled())
    panic("interruptible");

  if (--_k_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (_k_cpu()->irq_save_count == 0)
    k_arch_irq_state_restore(_k_cpu()->irq_flags);
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
  struct KCpu *my_cpu;

  _k_sched_lock();

  my_cpu = _k_cpu();

  if (my_cpu->lock_count <= 0)
    panic("lock_count <= 0");

  if (--my_cpu->lock_count == 0) {
    struct KTask *my_task = my_cpu->task;

    // Before resuming the current task, check whether it must give up the CPU
    // or exit.
    if ((my_task != NULL) && (my_task->flags & K_TASK_FLAG_RESCHEDULE)) {
      my_task->flags &= ~K_TASK_FLAG_RESCHEDULE;

      _k_sched_enqueue(my_task);
      _k_sched_yield_locked();
    }
  }

  _k_sched_unlock();
}
