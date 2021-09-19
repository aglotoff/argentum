#include <stdint.h>

#include "armv7.h"
#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "trap.h"
#include "vm.h"

static volatile uint32_t *gicc, *gicd;
static volatile uint32_t *ptimer;

void
gic_init(void)
{
  uint32_t *gic;
  
  gic    = (uint32_t *) vm_map_mmio(GIC_BASE, 4096 * 2);
  gicc   = gic + GICC;
  gicd   = gic + GICD;
  ptimer = gic + PTIMER;

  // Enable local PIC
  gicc[ICCICR] = ICCICR_EN;

  // Set priority mask
  gicc[ICCPMR] = 0xFF;

  // Enable global distributor
  gicd[ICDDCR] = ICDDCR_EN;
}

void
gic_enable(unsigned irq, unsigned cpu)
{
  (void) cpu;
  gicd[ICDISER0 + (irq >> 5)] = (1 << (irq & 0x1F));
  gicd[ICDIPR0 + (irq >> 2)]  = (0x80 << ((irq & 0x3) << 3));
  gicd[ICDIPTR0 + (irq >> 2)] = ((1 << cpu) << ((irq & 0x3) << 3));
}

unsigned
gic_intid(void)
{
  return gicc[ICCIAR];
}

void
gic_eoi(unsigned irq)
{
  gicc[ICCEOIR] = irq;
}

void
gic_start_others(void)
{
  gicd[ICDSGIR] = (1 << 24) | (0xF << 16);
}


/***** Private Timer *****/

#define PERIPHCLK 100000000U  // Peripheral clock rate, in Hz
#define TICK_RATE 100U        // Desired timer events rate, in Hz
#define PRESCALER 99U         // Prescaler value

/**
 * Setup the CPU private timer to generate interrupts at the rate of 100 Hz.
 *
 * This function must be called by each CPU.
 */
void
ptimer_init(void)
{
  ptimer[PTLOAD] = PERIPHCLK / ((PRESCALER + 1) * TICK_RATE) - 1;
  ptimer[PTCTRL] = (PRESCALER << 8) | PTCTRL_AUTO | PTCTRL_IRQEN | PTCTRL_EN;

  gic_enable(IRQ_PTIMER, read_mpidr() & 0x3);
}

/**
 * Clear the private timer pending interrupt.
 */
void
ptimer_eoi(void)
{
  ptimer[PTISR] = 1;
}
