#include <stdint.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/irq.h>

#include "gic.h"

// Interrupt distributor registers, divided by 4 for use as uint32_t[] indices
#define ICDDCR        (0x000 / 4)   // Distributor Control Register
  #define ICDDCR_EN     (1U << 0)   //    Enable
#define ICDISER0      (0x100 / 4)   // Interrupt Set-Enable Registers
#define ICDICER0      (0x180 / 4)   // Interrupt Clear-Enable Registers
#define ICDIPR0       (0x400 / 4)   // Interrupt Priority Registers
#define ICDIPTR0      (0x800 / 4)   // Interrupt Processor Targets Registers
#define ICDSGIR       (0xF00 / 4)   // Software Generated Interrupt Register

// CPU interface registers, divided by 4 for use as uint32_t[] indices
#define ICCICR        (0x000 / 4)   // CPU Interface Control Register
  #define ICCICR_EN     (1U << 0)   //   Enable Group 0 interrupts
#define ICCPMR        (0x004 / 4)   // Interrupt Priority Mask Register
#define ICCIAR        (0x00C / 4)   // Interrupt Acknowledge Register
#define ICCEOIR       (0x010 / 4)   // End of Interrupt Register

/*
 * ----------------------------------------------------------------------------
 * Interrupt Controller
 * ----------------------------------------------------------------------------
 * 
 * See ARM Generic Interrupt Controller Architecture Specification
 *
 */

void
gic_init(struct Gic *gic, void *icc_base, void *icd_base)
{ 
  gic->icc = (volatile uint32_t *) icc_base;
  gic->icd = (volatile uint32_t *) icd_base;
  
  gic_init_percpu(gic);
}

void
gic_init_percpu(struct Gic *gic)
{
  // Enable local PIC.
  gic->icc[ICCICR] = ICCICR_EN;

  // Set priority mask to the lowest possible value, so all interrupts can be
  // signalled to the processor.
  gic->icc[ICCPMR] = 0xFF;

  // Enable global distributor.
  gic->icd[ICDDCR] = ICDDCR_EN;
}

void
gic_enable(struct Gic *gic, unsigned irq, unsigned cpu)
{
  // Enable the interrupt
  gic->icd[ICDISER0 + (irq >> 5)] = (1U << (irq & 0x1F));

  // Set priority to 128 for all interrupts
  gic->icd[ICDIPR0  + (irq >> 2)] = (0x80 << ((irq & 0x3) << 3));

  // Set target CPU
  gic->icd[ICDIPTR0 + (irq >> 2)] = ((1U << cpu) << ((irq & 0x3) << 3));
}

void
gic_disable(struct Gic *gic, unsigned irq)
{
  gic->icd[ICDICER0 + (irq >> 5)] = (1U << (irq & 0x1F));
}

unsigned
gic_intid(struct Gic *gic)
{
  return gic->icc[ICCIAR] & 0x3FF;
}

void
gic_eoi(struct Gic *gic, unsigned irq)
{
  gic->icc[ICCEOIR] = irq;
}

void
gic_sgi(struct Gic *gic, unsigned irq)
{
  gic->icd[ICDSGIR] = (1 << 24) | (0xF << 16) | irq;
}
