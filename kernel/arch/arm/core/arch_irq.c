#include <kernel/core/config.h>
#include <kernel/core/irq.h>

#define PSR_F           (1 << 6)      // Fast interrupt disable bit
#define PSR_I           (1 << 7)      // Interrupt disable bit

// Disable both regular and fast IRQs
#define PSR_IRQ_MASK  (PSR_I | PSR_F)

static inline k_uint32_t
cpsr_get(void)
{
  k_uint32_t val;

  asm volatile("mrs %0, cpsr" : "=r" (val));
  return val;
}

/**
 * Set the value of the CPSR register.
 *
 * @param val The value to be set.
 */
static inline void
cpsr_set(k_uint32_t val)
{
  asm volatile("msr cpsr, %0" : : "r" (val));
}

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
  k_uint32_t cpsr = cpsr_get();

  cpsr_set(cpsr | PSR_IRQ_MASK);
  return ~cpsr & PSR_IRQ_MASK;
}

void
k_arch_irq_state_restore(int status)
{
  cpsr_set(cpsr_get() & ~status);
}
