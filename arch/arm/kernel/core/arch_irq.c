#include <kernel/armv7/regs.h>
#include <kernel/core/irq.h>

// Disable both regular and fast IRQs
#define PSR_IRQ_MASK  (PSR_I | PSR_F)

int
k_arch_irq_is_enabled(void)
{
  return (cpsr_get() & PSR_IRQ_MASK) != PSR_IRQ_MASK;
}

void
k_arch_irq_enable(void)
{
  cpsr_set(cpsr_get() & ~PSR_IRQ_MASK);
}

void
k_arch_irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_IRQ_MASK);
}

int
k_arch_irq_state_save(void)
{
  uint32_t cpsr = cpsr_get();

  cpsr_set(cpsr | PSR_IRQ_MASK);
  return ~cpsr & PSR_IRQ_MASK;
}

void
k_arch_irq_state_restore(int status)
{
  cpsr_set(cpsr_get() & ~status);
}
