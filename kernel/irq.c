#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include <argentum/process.h>
#include <argentum/armv7/regs.h>
#include <argentum/irq.h>
#include <argentum/drivers/gic.h>
#include <argentum/cprintf.h>

void
irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_I | PSR_F);
}

void
irq_enable(void)
{
  cpsr_set(cpsr_get() & ~(PSR_I | PSR_F));
}

/*
 * irq_save() and irq_restore() are used to disable and reenable interrupts on
 * the current CPU, respectively. Their invocations are counted, i.e. it takes
 * two irq_restore() calls to undo two irq_save() calls. This allows, for
 * example, to acquire two different locks and the interrupts will not be
 * reenabled until both locks have been released.
 */

/**
 * Save the current CPU interrupt state and disable interrupts.
 *
 * Both IRQ and FIQ interrupts are being disabled.
 */
void
irq_save(void)
{
  uint32_t psr;

  psr = cpsr_get();
  cpsr_set(psr | PSR_I | PSR_F);

  if (my_cpu()->irq_save_count++ == 0)
    my_cpu()->irq_flags = ~psr & (PSR_I | PSR_F);
}

/**
 * Restore the interrupt state saved by a preceding irq_save() call.
 */
void
irq_restore(void)
{
  uint32_t psr;

  psr = cpsr_get();
  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  if (--my_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (my_cpu()->irq_save_count == 0)
    cpsr_set(psr & ~my_cpu()->irq_flags);
}

static int (*irq_handlers[IRQ_MAX])(void);

int
irq_attach(int irq, int (*handler)(void), int cpu)
{
  if ((irq < 0) || (irq >= IRQ_MAX))
    return -EINVAL;
  if ((cpu < 0) || (cpu >= NCPU))
    return -EINVAL;

  irq_handlers[irq] = handler;
  gic_enable(irq, cpu);

  return 0;
}

void
irq_dispatch(void)
{
  int irq, resched;

  // Get the IRQ number and temporarily disable it
  irq = gic_intid();
  gic_disable(irq);
  gic_eoi(irq);

  // Enable nested IRQs
  irq_enable();

  resched = 0;

  if (irq_handlers[irq & 0xFFFFFF])
    resched = irq_handlers[irq & 0xFFFFFF]();
  else 
    cprintf("Unexpected IRQ %d from CPU %d\n", irq, cpu_id());

  // Disable nested IRQs
  irq_disable();

  // Re-enable the IRQ
  gic_enable(irq, cpu_id());

  if (resched && (my_thread() != NULL))
    kthread_yield();
}
