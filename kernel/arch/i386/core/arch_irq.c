#include <kernel/core/config.h>
#include <kernel/core/irq.h>

static inline k_uint32_t
eflags_get(void)
{
  k_uint32_t eflags;

  asm volatile("pushfl; popl %0" : "=r" (eflags));
  return eflags;
}

#define EFLAGS_IF         (1 << 9)    // Interrupt enable

int
k_arch_irq_is_enabled(void)
{
  return eflags_get() & EFLAGS_IF;
}

void
k_arch_irq_enable(void)
{
  asm volatile("sti");
}

void
k_arch_irq_disable(void)
{
  asm volatile("cli");
}

int
k_arch_irq_state_save(void)
{
  uint32_t eflags = eflags_get();

  asm volatile("cli");
  return eflags & EFLAGS_IF;
}

void
k_arch_irq_state_restore(int status)
{
  if (status & EFLAGS_IF)
    asm volatile("sti");
}
