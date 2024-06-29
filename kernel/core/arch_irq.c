#include <kernel/armv7/regs.h>
#include <kernel/irq.h>
#include <kernel/assert.h>

#include "core_private.h"

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
  // k_irq_save();

  // if (_k_cpu()->irq_save_count > 1)
  //   panic("should have uses k_irq_restore()");

  // k_irq_restore();

  cpsr_set(cpsr_get() & ~PSR_IRQ_MASK);
}

void
k_arch_irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_IRQ_MASK);
}

int
k_arch_irq_save(void)
{
  uint32_t cpsr = cpsr_get();

  cpsr_set(cpsr | PSR_IRQ_MASK);
  return ~cpsr & PSR_IRQ_MASK;
}

void
k_arch_irq_restore(int status)
{
  cpsr_set(cpsr_get() & ~status);
}
