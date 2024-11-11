#include <kernel/mach.h>
#include <kernel/interrupt.h>

void
arch_interrupt_ipi(void)
{
  mach_current->interrupt_ipi();
}

int
arch_interrupt_id(void)
{
  return mach_current->interrupt_id();
}

void
arch_interrupt_enable(int irq, int cpu)
{
  mach_current->interrupt_enable(irq, cpu);
}

void
arch_interrupt_mask(int irq)
{
  mach_current->interrupt_mask(irq);
}

void
arch_interrupt_unmask(int irq)
{
  mach_current->interrupt_unmask(irq);
}

void
arch_interrupt_init(void)
{
  mach_current->interrupt_init();
  mach_current->timer_init();
}

void
arch_interrupt_init_percpu(void)
{
  mach_current->interrupt_init_percpu();
  mach_current->timer_init_percpu();
}

void
arch_interrupt_eoi(int irq)
{
  mach_current->interrupt_eoi(irq);
}
