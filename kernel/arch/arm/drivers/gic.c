// See ARM Generic Interrupt Controller Architecture Specification

#include <arch/arm/gic.h>

// Interrupt distributor registers
#define ICDDCR        0x000       // Distributor Control Register
  #define ICDDCR_EN     (1U << 0) //    Enable
#define ICDISER0      0x100       // Interrupt Set-Enable Registers
#define ICDICER0      0x180       // Interrupt Clear-Enable Registers
#define ICDIPR0       0x400       // Interrupt Priority Registers
#define ICDIPTR0      0x800       // Interrupt Processor Targets Registers
#define ICDSGIR       0xF00       // Software Generated Interrupt Register

// CPU interface registers
#define ICCICR        0x000       // CPU Interface Control Register
  #define ICCICR_EN     (1U << 0) //   Enable Group 0 interrupts
#define ICCPMR        0x004       // Interrupt Priority Mask Register
#define ICCIAR        0x00C       // Interrupt Acknowledge Register
#define ICCEOIR       0x010       // End of Interrupt Register

static inline uint32_t
gic_icc_read(struct Gic *gic, uint32_t reg)
{
  return gic->icc[reg >> 2];
}

static inline void
gic_icc_write(struct Gic *gic, uint32_t reg, uint32_t data)
{
  gic->icc[reg >> 2] = data;
}

static inline void
gic_icd_write(struct Gic *gic, uint32_t reg, uint32_t data)
{
  gic->icd[reg >> 2] = data;
}

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
  gic_icc_write(gic, ICCICR, ICCICR_EN);

  // Set priority mask to the lowest possible value, so all interrupts can be
  // signalled to the processor.
  gic_icc_write(gic, ICCPMR, 0xFF);

  // Enable global distributor.
  gic_icd_write(gic, ICDDCR, ICDDCR_EN);
}

void
gic_setup(struct Gic *gic, unsigned irq, unsigned cpu)
{
  // Set priority to 128 for all interrupts
  gic_icd_write(gic, ICDIPR0  + irq, 0x80 << ((irq & 0x3) << 3));

  // Set target CPU
  gic_icd_write(gic, ICDIPTR0 + irq, (1U << cpu) << ((irq & 0x3) << 3));
}

void
gic_enable(struct Gic *gic, unsigned irq)
{
  gic_icd_write(gic, ICDISER0 + (irq >> 3), 1U << (irq & 0x1F));
}

void
gic_disable(struct Gic *gic, unsigned irq)
{
  gic_icd_write(gic, ICDICER0 + (irq >> 3), 1U << (irq & 0x1F));
}

unsigned
gic_intid(struct Gic *gic)
{
  return gic_icc_read(gic, ICCIAR) & 0x3FF;
}

void
gic_eoi(struct Gic *gic, unsigned irq)
{
  gic_icc_write(gic, ICCEOIR, irq);
}

void
gic_sgi(struct Gic *gic, unsigned irq)
{
  gic_icd_write(gic, ICDSGIR, (1 << 24) | (0xF << 16) | irq);
}
