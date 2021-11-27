#include <stdint.h>

#include "armv7.h"
#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "trap.h"
#include "vm.h"

#define GIC_BASE    0x1F000000      // GIC memory base address
#define GICC        (0x0100 / 4)    // Interrupt interface offset
#define PTIMER      (0x0600 / 4)    // Private timer offset
#define GICD        (0x1000 / 4)    // Distributor offset

static volatile uint32_t *gicc, *gicd, *ptimer;

// Interrupt distributor registers, divided by 4 for use as uint32_t[] indices
#define ICDDCR        (0x000 / 4)   // Distributor Control Register
  #define ICDDCR_EN     (1U << 0)   //    Enable
#define ICDISER0      (0x100 / 4)   // Interrupt Set-Enable Registers
#define ICDIPR0       (0x400 / 4)   // Interrupt Priority Registers
#define ICDIPTR0      (0x800 / 4)   // Interrupt Processor Targets Registers
#define ICDSGIR       (0xF00 / 4)   // Software Generated Interrupt Register

// CPU interface registers, divided by 4 for use as uint32_t[] indices
#define ICCICR        (0x000 / 4)   // CPU Interface Control Register
  #define ICCICR_EN     (1U << 0)   //   Enable Group 0 interrupts
#define ICCPMR        (0x004 / 4)   // Interrupt Priority Mask Register
#define ICCIAR        (0x00C / 4)   // Interrupt Acknowledge Register
#define ICCEOIR       (0x010 / 4)   // End of Interrupt Register

/*
 * ----------------------------------------------------------------------------
 * Interrupt Controller
 * ----------------------------------------------------------------------------
 * 
 * See ARM Generic Interrupt Controller Architecture Specification
 *
 */

void
gic_init(void)
{
  uint32_t *gic;
  
  gic    = (uint32_t *) vm_map_mmio(GIC_BASE, PAGE_SIZE * 2);
  gicc   = gic + GICC;
  gicd   = gic + GICD;
  ptimer = gic + PTIMER;

  // Enable local PIC.
  gicc[ICCICR] = ICCICR_EN;

  // Set priority mask.
  gicc[ICCPMR] = 0xFF;

  // Enable global distributor.
  gicd[ICDDCR] = ICDDCR_EN;
}

void
gic_enable(unsigned irq, unsigned cpu)
{
  (void) cpu;
  gicd[ICDISER0 + (irq >> 5)] = (1U << (irq & 0x1F));
  gicd[ICDIPR0  + (irq >> 2)] = (0x80 << ((irq & 0x3) << 3));
  gicd[ICDIPTR0 + (irq >> 2)] = ((1U << cpu) << ((irq & 0x3) << 3));
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
  gicd[ICDSGIR] = (1U << 24) | (0xF << 16);
}

/*
 * ----------------------------------------------------------------------------
 * Private timer
 * ----------------------------------------------------------------------------
 * 
 * See ARM(R) Cortex(R)-A9 MPCore Technical Reference Manual
 *
 */

// Private timer registers, divided by 4 for use as uint32_t[] indices
#define PTLOAD        (0x000 / 4)   // Private Timer Load Register
#define PTCOUNT       (0x004 / 4)   // Private Timer Counter Register
#define PTCTRL        (0x008 / 4)   // Private Timer Control Register
  #define PTCTRL_EN     (1U << 0)   // Timer Enable
  #define PTCTRL_AUTO   (1U << 1)   // Auto-reload mode
  #define PTCTRL_IRQEN  (1U << 2)   // IRQ Enable
#define PTISR         (0x00C / 4)   // Private Timer Interrupt Status Register

#define PERIPHCLK     100000000U    // Peripheral clock rate, in Hz
#define TICK_RATE     100U          // Desired timer events rate, in Hz
#define PRESCALER     99U           // Prescaler value

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
