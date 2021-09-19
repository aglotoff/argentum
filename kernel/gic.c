#include <stdint.h>

#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "vm.h"

static volatile uint32_t *gicc, *gicd;
static volatile uint32_t *sys;

void
gic_init(void)
{
  uint32_t *gic;
  
  gic = (uint32_t *) vm_map_mmio(GIC, 4096 * 2);
  gicc = &gic[GICC >> 2];
  gicd = &gic[GICD >> 2];

  sys = (uint32_t *) vm_map_mmio(SYS_BASE, 4096);

  // Enable local PIC
  gicc[ICCICR] = ICCICR_EN;

  // Set priority mask
  gicc[ICCPMR] = 0xFF;

  // Enable global distributor
  gicd[ICDDCR] = ICDDCR_EN;
}

void
gic_enable(unsigned irq, unsigned cpu)
{
  (void) cpu;
  gicd[ICDISER0 + (irq >> 5)] = (1 << (irq & 0x1F));
  gicd[ICDIPR0 + (irq >> 2)] = (0x80 << ((irq & 0x3) << 3));
  gicd[ICDIPTR0 + (irq >> 2)] = ((1 << cpu) << ((irq & 0x3) << 3));
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

void
gic_start_others(void)
{
  extern uint8_t _start[];

  sys[SYS_FLAGS] = (uint32_t) _start;
  gicd[ICDSGIR] = (1 << 24) | (0xF << 16);
}
