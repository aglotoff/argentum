#ifndef __AG_INCLUDE_ARCH_KERNEL_MPTIMER_H__
#define __AG_INCLUDE_ARCH_KERNEL_MPTIMER_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

/**
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

#endif  // !__AG_INCLUDE_ARCH_KERNEL_MPTIMER_H__
