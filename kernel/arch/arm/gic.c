// See ARM Generic Interrupt Controller Architecture Specification v2 (IHI0048B)

#include <gic.h>
#include <kernel.h>

// Distributor registers, divided by 4 for use as uint32_t[] indices
enum {
  GICD_CTLR       = (0x000 / 4),  // Distributor Control Register
  GICD_ISENABLER  = (0x100 / 4),  // Interrupt Set-Enable Registers
  GICD_ICENABLER  = (0x180 / 4),  // Interrupt Clear-Enable Registers
  GICD_IPRIORITYR = (0x400 / 4),  // Interrupt Priority Registers
  GICD_ITARGETSR  = (0x800 / 4),  // Interrupt Processor Targets Registers
  GICD_SGIR       = (0xF00 / 4),  // Software Generated Interrupt Register
};

// Distributor Control Register bits
enum {
  GICD_CTLR_ENABLE = (1U << 0),   // Enable
};

// CPU interface registers, divided by 4 for use as uint32_t[] indices
enum {
  GICC_CTLR      = (0x000 / 4),   // CPU Interface Control Register
  GICC_PMR       = (0x004 / 4),   // Interrupt Priority Mask Register
  GICC_IAR       = (0x00C / 4),   // Interrupt Acknowledge Register
  GICC_EOIR      = (0x010 / 4),   // End of Interrupt Register
};

// CPU Interface Control Register bits
enum {
  GICC_CTLR_ENABLE = (1U << 0)    // Enable signaling of interrupts
};

// Interrupt Priority Mask Register bits
enum {
  GICC_PMR_MAX = 0x00,  // Maximum priority level (no interrupts are signalled)
  GICC_PMR_MIN = 0xFF,  // Minimum priority level (all interrupts signalled)
};

/**
 * Initialize the GIC driver.
 *
 * @param gic      Pointer to the driver instance
 * @param icd_base Distributor base memory address
 * @param icc_base CPU Interface base memory address
 */
void
gic_init(struct Gic *gic, void *icd_base, void *icc_base)
{ 
  gic->icd = (volatile uint32_t *) icd_base;
  gic->icc = (volatile uint32_t *) icc_base;

  gic->icd[GICD_CTLR] = GICD_CTLR_ENABLE;

  gic_init_percpu(gic);
}

/**
 * Initialize the per-CPU GIC.
 *
 * @param gic Pointer to the driver instance
 */
void
gic_init_percpu(struct Gic *gic)
{
  gic->icc[GICC_CTLR] = GICC_CTLR_ENABLE;
  gic->icc[GICC_PMR]  = GICC_PMR_MIN;
}

/**
 * Configure an interrupt.
 * 
 * @param gic      Pointer to the driver instance
 * @param irq      The interrupt ID
 * @param priority Interrupt priority
 * @param cpu_list Target CPU mask
 */
void
gic_irq_config(struct Gic *gic, int irq, int priority, int cpu_list)
{
  int reg_number = irq >> 2;
  int reg_offset = (irq & 0x3) << 3;

  gic->icd[GICD_IPRIORITYR + reg_number] = (priority << reg_offset);
  gic->icd[GICD_ITARGETSR  + reg_number] = (cpu_list << reg_offset);
}

/**
 * Enable an interrupt.
 * 
 * @param gic Pointer to the driver instance
 * @param irq The interrupt ID
 */
void
gic_irq_unmask(struct Gic *gic, int irq)
{
  int reg_number = irq >> 5;
  int reg_offset = irq & 0x1F;

  gic->icd[GICD_ISENABLER + reg_number] = (1U << reg_offset);
}

/**
 * Disable an interrupt.
 * 
 * @param gic Pointer to the driver instance
 * @param irq The interrupt ID
 */
void
gic_irq_mask(struct Gic *gic, int irq)
{
  int reg_number = irq >> 5;
  int reg_offset = irq & 0x1F;

  gic->icd[GICD_ICENABLER + reg_number] = (1U << reg_offset);
}

/**
 * Acknowledge an interrupt to begin interrupt handling.
 * 
 * @param gic Pointer to the driver instance
 * @return Contents of the IAR register
 */
unsigned
gic_irq_ack(struct Gic *gic)
{
  return gic->icc[GICC_IAR];
}

/**
 * Send EOI to finish handling of an interrupt.
 *
 * @param gic Pointer to the driver instance
 * @param irq The interrupt ID
 */
void
gic_irq_eoi(struct Gic *gic, unsigned irq)
{
  gic->icc[GICC_EOIR] = irq;
}

void
gic_sgi(struct Gic *gic, int irq, int type, int cpu_mask)
{
  gic->icd[GICD_SGIR] = (type << 24) | ((cpu_mask & 0xFF) << 16) | (irq & 0xF);
}
