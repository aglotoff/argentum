#include <irq.h>
#include <smp.h>

/**
 * Get the current CPU structure. The caller must disable interrupts, otherwise
 * the task could move to a different processor due to timer interrupt. 
 *
 * @return Pointer to the current CPU structure.
 */
struct Cpu *
smp_cpu(void)
{
  if (arch_irq_is_enabled())
    panic("interruptible");

  return arch_smp_get_cpu(smp_id());
}
