#ifndef __KERNEL_DRIVERS_GIC_H__
#define __KERNEL_DRIVERS_GIC_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/gic.h
 * 
 * Generic Interrupt Controller.
 */

void     gic_init(void);
void     gic_init_percpu(void);
void     gic_enable(unsigned, unsigned);
void     gic_disable(unsigned);
unsigned gic_intid(void);
void     gic_eoi(unsigned);
void     gic_start_others(void);

void     ptimer_init(void);
void     ptimer_eoi(void);

#endif  // !__KERNEL_DRIVERS_GIC_H__
