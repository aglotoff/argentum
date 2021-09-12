#include <stdint.h>

#include "console.h"
#include "gic.h"
#include "memlayout.h"

static volatile uint32_t *gicc, *gicd;

void
gic_init(void)
{
  // TODO: map to reserved regions in the virtual address space.
  gicc = (volatile uint32_t *) (KERNEL_BASE + GICC);
  gicd = (volatile uint32_t *) (KERNEL_BASE + GICD);

  // Enable local PIC
  gicc[ICCICR] = ICCICR_EN;

  // Set priority mask
  gicc[ICCPMR] = 0xFF;

  // Enable global distributor
  gicd[ICDDCR] = ICDDCR_EN;
}

void
gic_enable(unsigned irq)
{
  gicd[ICDISER0 + (irq >> 5)] = (1 << (irq & 0x1F));
}

unsigned
gic_intid(void)
{
  return gicc[ICCIAR];
}

void
gic_eoi(unsigned irq)
{
  gicc[ICCEOIR] = irq;
}
