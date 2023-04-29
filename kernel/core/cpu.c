#include <assert.h>
#include <stddef.h>

#include <argentum/armv7/regs.h>
#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/irq.h>
#include <argentum/kthread.h>

struct Cpu _cpus[NCPU];

// Disable both regular and fast IRQs
#define PSR_IRQ_MASK  (PSR_I | PSR_F)

/**
 * Get the current CPU structure.
 * 
 * The caller must disable interrupts, otherwise the thread could switch to
 * a different processor due to timer interrupt and the return value will be
 * incorrect. 
 *
 * @return The pointer to the current CPU structure.
 */
struct Cpu *
cpu_current(void)
{
  uint32_t cpsr = cpsr_get();

  if ((cpsr & PSR_IRQ_MASK) != PSR_IRQ_MASK)
    panic("interruptible");

  // In case of four Cortex-A9 processors, the CPU IDs are 0x0, 0x1, 0x2, and
  // 0x3 so we can use them as array indicies.
  return &_cpus[cpu_id()];
}

/**
 * Disable all interrupts on the local (current) processor core.
 */
void
cpu_irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_IRQ_MASK);
}

/**
 * Enable all interrupts on the local (current) processor core.
 */
void
cpu_irq_enable(void)
{
  cpsr_set(cpsr_get() & ~PSR_IRQ_MASK);
}

/**
 * Save the current CPU interrupt state and disable interrupts.
 */
void
cpu_irq_save(void)
{
  uint32_t cpsr = cpsr_get();

  cpsr_set(cpsr | PSR_IRQ_MASK);

  if (cpu_current()->irq_save_count++ == 0)
    cpu_current()->irq_flags = ~cpsr & PSR_IRQ_MASK;
}

/**
 * Restore the interrupt state saved by a preceding cpu_irq_save() call.
 */
void
cpu_irq_restore(void)
{
  uint32_t cpsr = cpsr_get();

  if ((cpsr & PSR_IRQ_MASK) != PSR_IRQ_MASK)
    panic("interruptible");

  if (--cpu_current()->irq_save_count < 0)
    panic("interruptible");

  if (cpu_current()->irq_save_count == 0)
    cpsr_set(cpsr & ~cpu_current()->irq_flags);
}

/**
 * Notify the kernel that an ISR processing has started.
 */
void
isr_enter(void)
{
  uint32_t cpsr = cpsr_get();

  if ((cpsr & PSR_IRQ_MASK) != PSR_IRQ_MASK)
    panic("interruptible");

  cpu_current()->isr_nesting++;
}

/**
 * Notify the kernel that an ISR processing is finished.
 */
void
isr_exit(void)
{
  struct Cpu *my_cpu;

  sched_lock();

  my_cpu = cpu_current();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct KThread *my_thread = my_cpu->thread;

    if (my_thread != NULL) {
      // Before resuming the current thread, check whether it must give up the
      // CPU due to a higher-priority thread becoming available or due to time
      // quanta exhaustion.
      if (my_thread->flags & KTHREAD_RESCHEDULE) {
        my_thread->flags &= ~KTHREAD_RESCHEDULE;

        kthread_enqueue(my_thread);
        sched_yield();
      }

      // TODO: the thread could also be marked for desruction?
    }
  }

  sched_unlock();
}
