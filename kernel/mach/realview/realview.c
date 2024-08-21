#include <kernel/mach.h>
#include <kernel/vmspace.h>
#include <kernel/trap.h>
#include <kernel/irq.h>
#include <kernel/spinlock.h>

#include "../../drivers/rtc/ds1338.h"
#include "../../drivers/rtc/sbcon.h"
#include "../../gic.h"
#include "../../ptimer.h"
#include "../../sp804.h"

static struct Gic gic;
static struct PTimer ptimer;
static struct Sp804 timer01;

static void
realview_interrupt_ipi(void)
{
  gic_sgi(&gic, 0);
}

static int
realview_interrupt_id(void)
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

/*******************************************************************************
 * Time-of-Year RTC driver.
 * 
 * PBX-A9 has two serial bus inerfaces (SBCon0 and SBCon1). SBCon0 provides
 * access to the Maxim DS1338 RTC on the baseboard.
 ******************************************************************************/

// RTC device address on the I2C bus
#define RTC_ADDR    0xD0            

static struct SBCon sbcon0;
static struct DS1338 rtc;

static struct KSpinLock  rtc_lock;

/**
 * Initialize the RTC driver.
 */
static void
realview_rtc_init(void)
{
  sbcon_init(&sbcon0, PA2KVA(PHYS_CON0));
  ds1338_init(&rtc, &sbcon0, RTC_ADDR);
  k_spinlock_init(&rtc_lock, "rtc");
}

/**
 * Get the current UTC time.
 * 
 * @return The current UTC time.
 */
static time_t
realview_rtc_get_time(void)
{
  struct tm tm;

  k_spinlock_acquire(&rtc_lock);
  ds1338_get_time(&rtc, &tm);
  k_spinlock_release(&rtc_lock);

  return mktime(&tm);
}

/**
 * Set the current UTC time.
 * 
 * @param time  New time value.
 */
static void
realview_rtc_set_time(time_t time)
{
  k_spinlock_acquire(&rtc_lock);
  ds1338_get_time(&rtc, gmtime(&time));
  k_spinlock_release(&rtc_lock);
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

MACH_DEFINE(realview_pb_a8) {
  .type = MACH_REALVIEW_PB_A8,

  .interrupt_ipi         = realview_interrupt_ipi,
  .interrupt_id          = realview_interrupt_id,
  .interrupt_enable      = realview_interrupt_enable,
  .interrupt_init        = realview_interrupt_init_pb_a8,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask        = realview_interrupt_mask,
  .interrupt_unmask      = realview_interrupt_unmask,
  .interrupt_eoi         = realview_interrupt_eoi,

  .timer_init            = realview_pb_a8_timer_init,
  .timer_init_percpu     = realview_pb_a8_timer_init_percpu,

  .rtc_init              = realview_rtc_init,
  .rtc_get_time          = realview_rtc_get_time,
  .rtc_set_time          = realview_rtc_set_time,
};

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

MACH_DEFINE(realview_pbx_a9) {
  .type = MACH_REALVIEW_PBX_A9,

  .interrupt_ipi         = realview_interrupt_ipi,
  .interrupt_id          = realview_interrupt_id,
  .interrupt_enable      = realview_interrupt_enable,
  .interrupt_init        = realview_interrupt_init_pbx_a9,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask        = realview_interrupt_mask,
  .interrupt_unmask      = realview_interrupt_unmask,
  .interrupt_eoi         = realview_interrupt_eoi,

  .timer_init            = realview_pbx_a9_timer_init,
  .timer_init_percpu     = realview_pbx_a9_timer_init_percpu,

  .rtc_init              = realview_rtc_init,
  .rtc_get_time          = realview_rtc_get_time,
  .rtc_set_time          = realview_rtc_set_time,
};
