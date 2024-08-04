#ifndef __KERNEL_GIC_H__
#define __KERNEL_GIC_H__

/**
 * @file kernel/gic.h
 * 
 * Generic Interrupt Controller.
 */

#include <stdint.h>

struct Gic {
  volatile uint32_t *icc;
  volatile uint32_t *icd;
};

void     gic_init(struct Gic *, void *, void *);
void     gic_init_percpu(struct Gic *);
void     gic_setup(struct Gic *, unsigned, unsigned);
void     gic_enable(struct Gic *, unsigned);
void     gic_disable(struct Gic *, unsigned);
unsigned gic_intid(struct Gic *);
void     gic_eoi(struct Gic *, unsigned);
void     gic_sgi(struct Gic *, unsigned);

#endif  // !__KERNEL_GIC_H__
