#include <kernel/assert.h>
#include <stddef.h>

#include <kernel/armv7/regs.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/thread.h>

struct Cpu _cpus[NCPU];

// Disable both regular and fast IRQs
#define PSR_IRQ_MASK  (PSR_I | PSR_F)

/**
 * Get the current CPU structure.
 * 
 * The caller must disable interrupts, otherwise the task could switch to a
 * different processor due to timer interrupt and the return value will be
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
