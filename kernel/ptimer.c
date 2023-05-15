#include <stdint.h>

#include <kernel/mm/memlayout.h>
#include <kernel/mm/vm.h>
#include <kernel/irq.h>

#include "ptimer.h"

/*
 * ----------------------------------------------------------------------------
 * Private timer
 * ----------------------------------------------------------------------------
 * 
 * See ARM(R) Cortex(R)-A9 MPCore Technical Reference Manual
 *
 */

// Private timer registers, divided by 4 for use as uint32_t[] indices
#define LOAD          (0x000 / 4)   // Private Timer Load Register
#define COUNT         (0x004 / 4)   // Private Timer Counter Register
#define CTRL          (0x008 / 4)   // Private Timer Control Register
  #define CTRL_EN       (1U << 0)   // Timer Enable
  #define CTRL_AUTO     (1U << 1)   // Auto-reload mode
  #define CTRL_IRQEN    (1U << 2)   // IRQ Enable
#define ISR           (0x00C / 4)   // Private Timer Interrupt Status Register

#define PERIPHCLK     100000000U    // Peripheral clock rate, in Hz
#define TICK_RATE     100U          // Desired timer events rate, in Hz
#define PRESCALER     99U           // Prescaler value

/**
 * Setup the CPU private timer to generate interrupts at the rate of 100 Hz.
 *
 * This function must be called by each CPU.
 */
void
ptimer_init(struct PTimer *ptimer, void *base)
{
  ptimer->base = (volatile uint32_t *) base;

  ptimer_init_percpu(ptimer);
}

void
ptimer_init_percpu(struct PTimer *ptimer)
{
  ptimer->base[LOAD] = PERIPHCLK / ((PRESCALER + 1) * TICK_RATE) - 1;
  ptimer->base[CTRL] = (PRESCALER << 8) | CTRL_AUTO | CTRL_IRQEN | CTRL_EN;
}

/**
 * Clear the private timer pending interrupt.
 */
void
ptimer_eoi(struct PTimer *ptimer)
{
  ptimer->base[ISR] = 1;
}
