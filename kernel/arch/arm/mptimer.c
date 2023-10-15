// See Cortex-A9 MPCore Technical Reference Manual (DDI0407H)

#include <kernel.h>
#include <mptimer.h>

// Private timer registers, divided by 4 for use as uint32_t[] indices
enum {
  PT_LOAD    = (0x000 / 4),   // Private Timer Load Register
  PT_COUNTER = (0x004 / 4),   // Private Timer Counter Register
  PT_CONTROL = (0x008 / 4),   // Private Timer Control Register
  PT_ISR     = (0x00C / 4),   // Private Timer Interrupt Status Register
};

// Private Timer Control Register bits
enum {
  PT_CONTROL_ENABLE     = (1 << 0),   // Timer enabled
  PT_CONTROL_PERIODIC   = (1 << 1),   // Auto-reload mode
  PT_CONTROL_IRQ_ENABLE = (1 << 2),   // Interrupt enabled
};

#define PRESCALER     99U             // Prescaler value

/**
 * Setup the private timer driver instance.
 * 
 * @param mptimer         Pointer to the driver instance
 * @param base            Memory base address
 * @param cycles_per_tick The number of clock cycles per tick
 */
void
mptimer_init(struct MPTimer *mptimer, void *base, unsigned long cycles_per_tick)
{
  mptimer->regs = (volatile uint32_t *) base;
  mptimer_init_percpu(mptimer, cycles_per_tick);
}

/**
 * Setup the per-CPU private timer.
 * 
 * @param mptimer         Pointer to the driver instance
 * @param cycles_per_tick The number of clock cycles per tick
 */
void
mptimer_init_percpu(struct MPTimer *mptimer, unsigned long cycles_per_tick)
{
  mptimer->regs[PT_LOAD]    = cycles_per_tick / (PRESCALER + 1) - 1;
  mptimer->regs[PT_CONTROL] = PT_CONTROL_ENABLE
                            | PT_CONTROL_PERIODIC
                            | PT_CONTROL_IRQ_ENABLE
                            | (PRESCALER << 8);
}

/**
 * Clear the pending private timer interrupt.
 * 
 * @param mptimer Pointer to the driver instance
 */
void
mptimer_eoi(struct MPTimer *mptimer)
{
  mptimer->regs[PT_ISR] = 1;
}
