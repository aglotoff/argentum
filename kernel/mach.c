#include <kernel/mach.h>
#include <kernel/vmspace.h>
#include <kernel/trap.h>
#include <kernel/irq.h>

#include "gic.h"
#include "ptimer.h"
#include "sp804.h"

static struct Gic gic;
static struct PTimer ptimer;
static struct Sp804 timer01;

static void
realview_interrupt_ipi(void)
{
  gic_sgi(&gic, 0);
}

static int
realview_irq_id(void)
{
  return gic_intid(&gic);
}

static void
realview_interrupt_enable(int irq, int cpu)
{
  gic_setup(&gic, irq, cpu);
}

static void
realview_interrupt_mask(int irq)
{
  gic_disable(&gic, irq);
}

static void
realview_interrupt_unmask(int irq)
{
  gic_enable(&gic, irq);
}

static void
realview_interrupt_init_pb_a8(void)
{
  gic_init(&gic, PA2KVA(0x1E000000), PA2KVA(0x1E001000));
}

static void
realview_interrupt_init_pbx_a9(void)
{
  gic_init(&gic, PA2KVA(0x1F000100), PA2KVA(0x1F001000));

  k_irq_attach(0, ipi_irq, NULL);

  *(volatile int *) PA2KVA(0x10000030) = 0x10000;
  gic_sgi(&gic, 0);
}

static void
realview_interrupt_init_percpu(void)
{
  gic_init_percpu(&gic);
  interrupt_unmask(0);
}

static void
realview_interrupt_eoi(int irq)
{
  gic_eoi(&gic, irq);
}

static int
realview_pbx_a9_timer_irq(void *arg)
{
  ptimer_eoi(&ptimer);
  return timer_irq(arg);
}

static void
realview_pbx_a9_timer_init(void)
{
  ptimer_init(&ptimer, PA2KVA(0x1F000600));
  k_irq_attach(29, realview_pbx_a9_timer_irq, NULL);
}

static void
realview_pbx_a9_timer_init_percpu(void)
{
  ptimer_init_percpu(&ptimer);
  interrupt_unmask(29);
}

static int
realview_pb_a8_timer_irq(void *arg)
{
  sp804_eoi(&timer01);
  return timer_irq(arg);
}

static void
realview_pb_a8_timer_init(void)
{
  sp804_init(&timer01, PA2KVA(0x10011000));
  k_irq_attach(36, realview_pb_a8_timer_irq, NULL);
}

static void
realview_pb_a8_timer_init_percpu(void)
{

}

int mach_type;

static struct Machine mach_realview_pb_a8 = {
  .interrupt_ipi = realview_interrupt_ipi,
  .irq_id = realview_irq_id,
  .interrupt_enable = realview_interrupt_enable,
  .interrupt_init = realview_interrupt_init_pb_a8,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask = realview_interrupt_mask,
  .interrupt_unmask = realview_interrupt_unmask,
  .interrupt_eoi = realview_interrupt_eoi,

  .timer_init = realview_pb_a8_timer_init,
  .timer_init_percpu = realview_pb_a8_timer_init_percpu,
};

static struct Machine mach_realview_pbx_a9 = {
  .interrupt_ipi = realview_interrupt_ipi,
  .irq_id = realview_irq_id,
  .interrupt_enable = realview_interrupt_enable,
  .interrupt_init = realview_interrupt_init_pbx_a9,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask = realview_interrupt_mask,
  .interrupt_unmask = realview_interrupt_unmask,
  .interrupt_eoi = realview_interrupt_eoi,

  .timer_init = realview_pbx_a9_timer_init,
  .timer_init_percpu = realview_pbx_a9_timer_init_percpu,
};

struct Machine *mach[5108] = {
  [MACH_REALVIEW_PB_A8] &mach_realview_pb_a8,
  [MACH_REALVIEW_PBX_A9] &mach_realview_pbx_a9,
};
