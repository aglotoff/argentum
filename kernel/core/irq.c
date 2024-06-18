#include <kernel/assert.h>
#include <errno.h>
#include <stddef.h>

#include <kernel/process.h>
#include <kernel/irq.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>

#include "core_private.h"

/**
 * Save the current CPU interrupt state and disable interrupts.
 */
void
k_irq_save(void)
{
  int status = k_arch_irq_save();

  if (_k_cpu()->irq_save_count++ == 0)
    _k_cpu()->irq_flags = status;
}

/**
 * Restore the interrupt state saved by a preceding k_irq_save() call.
 */
void
k_irq_restore(void)
{
  if (k_arch_irq_is_enabled())
    panic("interruptible");

  if (--_k_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (_k_cpu()->irq_save_count == 0)
    k_arch_irq_restore(_k_cpu()->irq_flags);
}

/**
 * Notify the kernel that an ISR processing has started.
 */
void
k_irq_begin(void)
{
  _k_sched_spin_lock();
  _k_cpu()->lock_count++;
  _k_sched_spin_unlock();
}

/**
 * Notify the kernel that an ISR processing is finished.
 */
void
k_irq_end(void)
{
  struct KCpu *my_cpu;

  _k_sched_spin_lock();

  my_cpu = _k_cpu();

  if (my_cpu->lock_count <= 0)
    panic("lock_count <= 0");

  if (--my_cpu->lock_count == 0) {
    struct KThread *my_thread = my_cpu->thread;

    // Before resuming the current thread, check whether it must give up the CPU
    // or exit.
    if ((my_thread != NULL) && (my_thread->flags & THREAD_FLAG_RESCHEDULE)) {
      my_thread->flags &= ~THREAD_FLAG_RESCHEDULE;

      _k_sched_enqueue(my_thread);
      _k_sched_yield();
    }
  }

  _k_sched_spin_unlock();
}

static k_irq_handler_t irq_handlers[IRQ_MAX];

k_irq_handler_t
k_irq_attach(int irq, k_irq_handler_t handler)
{
  k_irq_handler_t old_handler;

  old_handler = irq_handlers[irq];
  irq_handlers[irq] = handler;
  irq_unmask(irq);

  return old_handler;
}

void
k_irq_dispatch(int irq)
{
  if (irq_handlers[irq])
    irq_handlers[irq]();
  else 
    cprintf("Unexpected IRQ %d from CPU %d\n", irq, k_cpu_id());
}
