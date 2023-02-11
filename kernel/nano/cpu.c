#include <assert.h>
#include <stddef.h>

#include <argentum/armv7/regs.h>
#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/irq.h>
#include <argentum/kthread.h>

/*******************************************************************************
 * Per-CPU state
 * 
 * The caller must disable interrupts while accessing the per-CPU data, since a
 * timer IRQ may cause the current thread to be moved to another processor, and
 * the pointer to the CPU struct will no longer be valid.
 ******************************************************************************/

struct Cpu cpus[NCPU];

/**
 * Get the current processor ID.
 * 
 * @return The current processor ID.
 */
unsigned
cpu_id(void)
{
  return cp15_mpidr_get() & 3;
}

/**
 * Get the current CPU structure.
 *
 * @return The pointer to the current CPU structure.
 */
struct Cpu *
my_cpu(void)
{
  uint32_t psr;

  psr = cpsr_get();
  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  return &cpus[cpu_id()];
}

struct KThread *
my_thread(void)
{
  struct KThread *thread;

  cpu_irq_save();
  thread = my_cpu()->thread;
  cpu_irq_restore();

  return thread;
}

/*******************************************************************************
 * Interrupt control
 * 
 * cpu_irq_save() and cpu_irq_restore() are used to disable and reenable
 * interrupts on the current CPU, respectively. Their invocations are counted,
 * i.e. it takes two cpu_irq_restore() calls to undo two cpu_irq_save() calls.
 * This allows, for example, to acquire two different locks and the interrupts
 * will not be reenabled until both locks have been released.
 ******************************************************************************/

void
cpu_irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_I | PSR_F);
}

void
cpu_irq_enable(void)
{
  cpsr_set(cpsr_get() & ~(PSR_I | PSR_F));
}

/**
 * Save the current CPU interrupt state and disable interrupts.
 *
 * Both IRQ and FIQ interrupts are being disabled.
 */
void
cpu_irq_save(void)
{
  uint32_t psr;

  psr = cpsr_get();
  cpsr_set(psr | PSR_I | PSR_F);

  if (my_cpu()->irq_save_count++ == 0)
    my_cpu()->irq_flags = ~psr & (PSR_I | PSR_F);
}

/**
 * Restore the interrupt state saved by a preceding cpu_irq_save() call.
 */
void
cpu_irq_restore(void)
{
  uint32_t psr = cpsr_get();

  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  if (--my_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (my_cpu()->irq_save_count == 0)
    cpsr_set(psr & ~my_cpu()->irq_flags);
}

void
cpu_isr_enter(void)
{
  uint32_t psr = cpsr_get();

  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  my_cpu()->isr_nesting++;
}

void
cpu_isr_exit(void)
{
  struct KThread *th;

  sched_lock();

  th = my_cpu()->thread;

  if (my_cpu()->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if ((--my_cpu()->isr_nesting == 0) && (th != NULL)) {
    if (th->flags & KTHREAD_RESCHEDULE) {
      th->flags &= ~KTHREAD_RESCHEDULE;
      kthread_list_add(th);
      sched_yield();
    }
  }

  sched_unlock();
}
