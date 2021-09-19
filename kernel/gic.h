#ifndef KERNEL_GIC_H
#define KERNEL_GIC_H

/**
 * @file kernel/gic.h
 * 
 * Generic Interrupt Controller.
 * 
 * See Arm Generic Interrupt Controller Architecture Specification
 */

#define GIC_BASE    0x1F000000      ///< GIC memory base address
#define GICC        (0x0100 >> 2)   ///< Interrupt interface offset
#define PTIMER      (0x0600 >> 2)   ///< Private timer offset
#define GICD        (0x1000 >> 2)   ///< Distributor offset

/** @name GICDRegs
 *  Interrupt distributor registers, shifted right by 2 bits for use as
 *  uint32_t[] indices
 */
///@{
#define ICDDCR        (0x000 >> 2)  ///< Distributor Control Register
  #define ICDDCR_EN     (1 << 0)    ///<    Enable
#define ICDISER0      (0x100 >> 2)  ///< Interrupt Set-Enable Registers
#define ICDIPR0       (0x400 >> 2)  ///< Interrupt Priority Registers
#define ICDIPTR0      (0x800 >> 2)  ///< Interrupt Processor Targets Registers
#define ICDSGIR       (0xF00 >> 2)  ///< Software Generated Interrupt Register
///@}

/** @name GICCRegs
 *  CPU interface registers, shifted right by 2 bits for use as uint32_t[]
 *  indices
 */
///@{
#define ICCICR        (0x000 >> 2)  ///< CPU Interface Control Register
  #define ICCICR_EN     (1 << 0)    ///<   Enable Group 0 interrupts
#define ICCPMR        (0x004 >> 2)  ///< Interrupt Priority Mask Register
#define ICCIAR        (0x00C >> 2)  ///< Interrupt Acknowledge Register
#define ICCEOIR       (0x010 >> 2)  ///< End of Interrupt Register
///@}

/** @name PTRegs
 *  Private timer registers, shifted right by 2 bits for use as uint32_t[]
 *  indices
 */
///@{
#define PTLOAD        (0x000 >> 2)  ///< Private Timer Load Register
#define PTCOUNT       (0x004 >> 2)  ///< Private Timer Counter Register
#define PTCTRL        (0x008 >> 2)  ///< Private Timer Control Register
  #define PTCTRL_EN     (1 << 0)    ///< Timer Enable
  #define PTCTRL_AUTO   (1 << 1)    ///< Auto-reload mode
  #define PTCTRL_IRQEN  (1 << 2)    ///< IRQ Enable
#define PTISR         (0x00C >> 2)  ///< Private Timer Interrupt Status Register
///@}

void     gic_init(void);
void     gic_init_percpu(void);
void     gic_enable(unsigned irq, unsigned cpu);
unsigned gic_intid(void);
void     gic_eoi(unsigned irq);
void     gic_start_others(void);

void     ptimer_init(void);
void     ptimer_eoi(void);

#endif  // !KERNEL_GIC_H
