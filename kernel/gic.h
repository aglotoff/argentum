#ifndef KERNEL_GIC_H
#define KERNEL_GIC_H

// See Arm Generic Interrupt Controller Architecture Specification

#define GICC        0x1F000100      ///< Interrupt interface base address
#define PTIMER      0x1F000600      ///< Private timer base address
#define GICD        0x1F001000      ///< Distributor base address

// GIC distributor registers, shifted right by 2 bits for use as uint32_t[]
// indicies
#define ICDDCR        (0x000 >> 2)  ///< Distributor Control Register
  #define ICDDCR_EN     (1 << 0)    ///<    Enable
#define ICDISER0      (0x100 >> 2)  ///< Interrupt Set-Enable Registers
#define ICDIPTR0      (0x800 >> 2)  ///< Interrupt Processor Targets Registers

// GIC CPU interface registers, shifted right by 2 bits for use as uint32_t[]
// indicies
#define ICCICR        (0x000 >> 2)  ///< CPU Interface Control Register
  #define ICCICR_EN     (1 << 0)    ///<   Enable Group 0 interrupts
#define ICCPMR        (0x004 >> 2)  ///< Interrupt Priority Mask Register
#define ICCIAR        (0x00C >> 2)  ///< Interrupt Acknowledge Register
#define ICCEOIR       (0x010 >> 2)  ///< End of Interrupt Register

/**
 * Initialize the interrupt controller.
 */
void gic_init(void);

/**
 * Enable the specified interrupt.
 */
void gic_enable(unsigned irq);

/**
 * Returns ID of the signaled interrupt.
 */
unsigned gic_intid(void);

/**
 * Deactivate the specified interrupt.
 */
void gic_eoi(unsigned irq);

#endif  // !KERNEL_GIC_H
