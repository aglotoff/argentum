#ifndef __AG_KERNEL_MPTIMER_H__
#define __AG_KERNEL_MPTIMER_H__

/**
 * @file kernel/mptimer.h
 * 
 * ARM Cortex-A9 private timer driver.
 */

#include <stdint.h>

/** Private Timer driver instance */
struct MPTimer {
  /** Base address */
  volatile uint32_t *regs;
};

void mptimer_init(struct MPTimer *, void *, unsigned long);
void mptimer_init_percpu(struct MPTimer *, unsigned long);
void mptimer_eoi(struct MPTimer *);

#endif  // !__AG_KERNEL_MPTIMER_H__
