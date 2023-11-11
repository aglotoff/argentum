#include <kernel/irq.h>
#include <kernel/smp.h>
#include <kernel/vm.h>

#include <arch/kernel/gic.h>
#include <arch/kernel/mptimer.h>
#include <arch/kernel/realview_pbx_a9.h>
#include <arch/kernel/regs.h>

#define PERIPHCLK     100000000U    // Peripheral clock rate, in Hz
#define TICK_RATE     100U          // Desired timer events rate, in Hz

static struct Gic gic;

static struct MPTimer mptimer;
static struct IrqHook mptimer_hook;

static int
mptimer_handle_irq(int irq)
{
  (void) irq;

  mptimer_eoi(&mptimer);
  
  // TODO: update kernel timeouts

 kprintf("Tick on CPU %d\n", smp_id());

  return 1;
}

void
arch_irq_init(void)
{
  // Initialize the interrupt controller
  gic_init(&gic, PA2KVA(GICD_BASE), PA2KVA(GICC_BASE));

  // Initialize the private timer and register the timer IRQ handler
  mptimer_init(&mptimer, PA2KVA(MPTIMER_BASE), PERIPHCLK / TICK_RATE);
  irq_hook_attach(&mptimer_hook, MPTIMER_IRQ, mptimer_handle_irq);
}

void
arch_irq_init_percpu(void)
{
  // Initialize the interrupt controller on this CPU
  gic_init_percpu(&gic);

  // Initialize the private timer and enable timer IRQ on this CPU
  mptimer_init_percpu(&mptimer, PERIPHCLK / TICK_RATE);
  irq_hook_enable(&mptimer_hook);
}

// Disable both regular and fast IRQs
#define PSR_IRQ_MASK  (PSR_I | PSR_F)

int
arch_irq_is_enabled(void)
{
  return (cpsr_get() & PSR_IRQ_MASK) != PSR_IRQ_MASK;
}

void
arch_irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_IRQ_MASK);
}

void
arch_irq_enable(void)
{
  cpsr_set(cpsr_get() & ~PSR_IRQ_MASK);
}

int
arch_irq_save(void)
{
  uint32_t cpsr = cpsr_get();

  cpsr_set(cpsr | PSR_IRQ_MASK);
  return ~cpsr & PSR_IRQ_MASK;
}

void
arch_irq_restore(int flags)
{
  cpsr_set(cpsr_get() & ~flags);
}

void
arch_irq_unmask(int irq)
{
  // Assign the same priority to all interrupts, set the current CPU as target
  gic_irq_config(&gic, irq, 0x80, (1 << smp_id()));
  gic_irq_unmask(&gic, irq);
}

void
arch_irq_mask(int irq)
{
  gic_irq_mask(&gic, irq);
}

void
arch_irq_dispatch(void)
{
  int irq_id = gic_irq_ack(&gic);

  irq_handle(irq_id & 0x3FF);

  gic_irq_eoi(&gic, irq_id);
}

void
arch_irq_ipi_single(int irq, int cpu)
{
  gic_sgi(&gic, irq, GIC_SGI_FILTER_TARGET, 1 << cpu);
}

void
arch_irq_ipi_others(int irq)
{
  gic_sgi(&gic, irq, GIC_SGI_FILTER_OTHERS, 0);
}

void
arch_irq_ipi_self(int irq)
{
  gic_sgi(&gic, irq, GIC_SGI_FILTER_SELF, 0);
}
