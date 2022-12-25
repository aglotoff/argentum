#ifndef __KERNEL_PTIMER_H__
#define __KERNEL_PTIMER_H__

#include <stdint.h>

struct PTimer {
  volatile uint32_t *base;
};

void     ptimer_init(struct PTimer*, void *base);
void     ptimer_init_percpu(struct PTimer *);
void     ptimer_eoi(struct PTimer *);

#endif  // !__KERNEL_PTIMER_H__
