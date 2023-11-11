#ifndef __AG_INCLUDE_ARCH_KERNEL_GIC_H__
#define __AG_INCLUDE_ARCH_KERNEL_GIC_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <stdint.h>

/** GIC driver instance */
struct Gic {
  /** Distributor base address */
  volatile uint32_t *icd;
  /** CPU interface base address */
  volatile uint32_t *icc;
};

/**
 * Retrieve the pending interrupt ID from the contents of the IAR register
 * returned by gic_irq_ack()
 */
#define GIC_IRQ_ID(iar) ((iar) & 0x3FF)

enum {
  /** Forward SGI to specified CPUs */
  GIC_SGI_FILTER_TARGET = 0,
  /** Forward SGI to all CPUs except the current one */
  GIC_SGI_FILTER_OTHERS = 1,
  /** Forward SGI only to the current CPU */
  GIC_SGI_FILTER_SELF   = 2,
};

void     gic_init(struct Gic *, void *, void *);
void     gic_init_percpu(struct Gic *);
void     gic_irq_unmask(struct Gic *, int);
void     gic_irq_mask(struct Gic *, int);
void     gic_irq_config(struct Gic *, int, int, int);
unsigned gic_irq_ack(struct Gic *);
void     gic_irq_eoi(struct Gic *, unsigned);
void     gic_sgi(struct Gic *, int, int, int);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_GIC_H__
