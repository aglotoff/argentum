#ifndef __INCLUDE_ARGENTUM_DRIVERS_GIC_H__
#define __INCLUDE_ARGENTUM_DRIVERS_GIC_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/argentum/drivers/gic.h
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
void     ptimer_intr(void);

#endif  // !__INCLUDE_ARGENTUM_DRIVERS_GIC_H__
