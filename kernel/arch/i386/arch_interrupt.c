#include <kernel/interrupt.h>

void
arch_interrupt_ipi(void)
{
  // TODO
}

int
arch_interrupt_id(void)
{
  // TODO
  return -1;
}

void
arch_interrupt_enable(int irq, int cpu)
{
  // TODO
  (void) irq;
  (void) cpu;
}

void
arch_interrupt_mask(int irq)
{
  // TODO
  (void) irq;
}

void
arch_interrupt_unmask(int irq)
{
  // TODO
  (void) irq;
}

void
arch_interrupt_init(void)
{
  // TODO
}

void
arch_interrupt_init_percpu(void)
{
  // TODO
}

void
arch_interrupt_eoi(int irq)
{
  // TODO
  (void) irq;
}
