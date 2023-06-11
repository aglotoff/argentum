#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include <kernel/process.h>
#include <kernel/armv7/regs.h>
#include <kernel/irq.h>
#include <kernel/cprintf.h>
#include <kernel/ktimer.h>
#include <kernel/mm/memlayout.h>
#include <kernel/mm/vm.h>

#include "gic.h"
#include "ptimer.h"

static struct Gic gic;
static struct PTimer ptimer;

static void
ptimer_irq(void)
{
  ptimer_eoi(&ptimer);

  if (cpu_id() == 0)
    ktimer_tick();

  sched_tick();
}

static void
ipi_irq(void)
{
  return;
}

void
irq_ipi(void)
{
  gic_sgi(&gic, 0);
}

void
irq_init(void)
{
  gic_init(&gic, PA2KVA(PHYS_GICC), PA2KVA(PHYS_GICD));
  ptimer_init(&ptimer, PA2KVA(PHYS_PTIMER));

  irq_attach(IRQ_PTIMER, ptimer_irq, cp15_mpidr_get() & 0x3);
  irq_attach(0, ipi_irq, cp15_mpidr_get() & 0x3);
}

void
irq_init_percpu(void)
{
  gic_init_percpu(&gic);
  ptimer_init_percpu(&ptimer);

  gic_enable(&gic, IRQ_PTIMER, cp15_mpidr_get() & 0x3);
  gic_enable(&gic, 0, cp15_mpidr_get() & 0x3);
}

static void (*irq_handlers[IRQ_MAX])(void);

int
irq_attach(int irq, void (*handler)(void), int cpu)
{
  if ((irq < 0) || (irq >= IRQ_MAX))
    return -EINVAL;
  if ((cpu < 0) || (cpu >= NCPU))
    return -EINVAL;

  irq_handlers[irq] = handler;
  gic_enable(&gic, irq, cpu);

  return 0;
}

void
irq_dispatch(void)
{
  int irq;

  sched_isr_enter();

  // Get the IRQ number and temporarily disable it
  irq = gic_intid(&gic);
  gic_disable(&gic, irq);
  gic_eoi(&gic, irq);

  // Enable nested IRQs
  cpu_irq_enable();

  if (irq_handlers[irq & 0xFFFFFF])
    irq_handlers[irq & 0xFFFFFF]();
  else 
    cprintf("Unexpected IRQ %d from CPU %d\n", irq, cpu_id());

  // Disable nested IRQs
  // cpu_irq_disable();

  // Re-enable the IRQ
  gic_enable(&gic, irq, cpu_id());

  sched_isr_exit();
}
