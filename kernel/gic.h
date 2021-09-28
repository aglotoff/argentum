#ifndef __KERNEL_GIC_H__
#define __KERNEL_GIC_H__

/**
 * @file kernel/gic.h
 * 
 * Generic Interrupt Controller.
 */

void     gic_init(void);
void     gic_init_percpu(void);
void     gic_enable(unsigned, unsigned);
unsigned gic_intid(void);
void     gic_eoi(unsigned);
void     gic_start_others(void);

void     ptimer_init(void);
void     ptimer_eoi(void);

#endif  // !__KERNEL_GIC_H__
