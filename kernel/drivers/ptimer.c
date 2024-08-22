// See ARM(R) Cortex(R)-A9 MPCore Technical Reference Manual

#include <kernel/drivers/ptimer.h>

// Private timer registers
#define LOAD          0x000   // Private Timer Load Register
#define COUNT         0x004   // Private Timer Counter Register
#define CTRL          0x008   // Private Timer Control Register
  #define CTRL_EN       (1U << 0)   // Timer Enable
  #define CTRL_AUTO     (1U << 1)   // Auto-reload mode
  #define CTRL_IRQEN    (1U << 2)   // IRQ Enable
#define ISR           0x00C   // Private Timer Interrupt Status Register

#define PERIPHCLK     100000000U    // Peripheral clock rate, in Hz
#define PRESCALER     99U           // Prescaler value

static inline void
ptimer_write(struct PTimer *ptimer, uint32_t reg, uint32_t data)
{
  ptimer->base[reg >> 2] = data;
}

void
ptimer_init(struct PTimer *ptimer, void *base)
{
  ptimer->base = (volatile uint32_t *) base;
}

void
ptimer_init_percpu(struct PTimer *ptimer, int rate)
{
  ptimer_write(ptimer, LOAD, PERIPHCLK / ((PRESCALER + 1) * rate) - 1);
  ptimer_write(ptimer, CTRL, (PRESCALER << 8) |
                             CTRL_AUTO |
                             CTRL_IRQEN |
                             CTRL_EN);
}

/**
 * Clear the private timer pending interrupt.
 */
void
ptimer_eoi(struct PTimer *ptimer)
{
  ptimer_write(ptimer, ISR, 1);
}
