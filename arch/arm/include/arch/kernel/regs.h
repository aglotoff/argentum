#ifndef __AG_INCLUDE_ARCH_KERNEL_REGS_H__
#define __AG_INCLUDE_ARCH_KERNEL_REGS_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

/**
 * 
 * This file contains definitions for core and system registers as well as 
 * routines to let C code access those registers.
 */

/** Mode field bitmask */
#define PSR_M_MASK      0x1F
/** User mode */
#define PSR_M_USR       0x10
/** FIQ mode */
#define PSR_M_FIQ       0x11
/** IRQ mode */
#define PSR_M_IRQ       0x12
/** Supervisor mode */
#define PSR_M_SVC       0x13
/** Monitor mode */
#define PSR_M_MON       0x16
/** Abort mode */
#define PSR_M_ABT       0x17
/** Undefined mode */
#define PSR_M_UND       0x1B
/** System mode */
#define PSR_M_SYS       0x1F
/** Thumb execution state bit */
#define PSR_T           (1 << 5)
/** Fast interrupt disable bit */
#define PSR_F           (1 << 6)
/** Interrupt disable bit */
#define PSR_I           (1 << 7)
/** Asynchronous abort disable bit */
#define PSR_A           (1 << 8)
/** Endianness execution state bit */
#define PSR_E           (1 << 9)
/** Greater than or Equal flags bitmask */
#define PSR_GE_MASK     (0xF << 16)
/** Jazelle bit */
#define PSR_J           (1 << 24)
/** Cumulative saturation flag */
#define PSR_Q           (1 << 27)
/** Overflow condition code flag */
#define PSR_V           (1 << 28)
/** Carry condition code flag */
#define PSR_C           (1 << 29)
/** Zero condition code flag */
#define PSR_Z           (1 << 30)
/** Negative condition code flag */
#define PSR_N           (1 << 31)

/** Multiprocessor Affinity Register  */
#define CP15_MPIDR(x)   p15, 0, x, c0, c0, 5
/** System Control Register */
#define CP15_SCTLR(x)   p15, 0, x, c1, c0, 0
/** Coprocessor Access Control Register */
#define CP15_CPACR(x)   p15, 0, x, c1, c0, 2
/** Translation Table Base 0 Register */
#define CP15_TTBR0(x)   p15, 0, x, c2, c0, 0
/** Translation Table Base 1 Register */
#define CP15_TTBR1(x)   p15, 0, x, c2, c0, 1
/** Translation Table Base Control Register */
#define CP15_TTBCR(x)   p15, 0, x, c2, c0, 2
/** Data Fault Status Register */
#define CP15_DFSR(x)    p15, 0, x, c5, c0, 0
/** Instruction Fault Status Register */
#define CP15_IFSR(x)    p15, 0, x, c5, c0, 1
/** Data Fault Address Register */
#define CP15_DFAR(x)    p15, 0, x, c6, c0, 0 
/** Instruction Fault Address Register */
#define CP15_IFAR(x)    p15, 0, x, c6, c0, 1
/** Domain Access Control Register */
#define CP15_DACR(x)    p15, 0, x, c3, c0, 0

/** MMU enable */
#define SCTLR_M      (1 << 0)
/** Alignment */
#define SCTLR_A      (1 << 1)
/** Cache enable */
#define SCTLR_C      (1 << 2)
/** SWP/SWPB Enable */
#define SCTLR_SW     (1 << 10)
/** Branch prediction enable */
#define SCTLR_Z      (1 << 11)
/** Instruction cache enable */
#define SCTLR_I      (1 << 12)
/** High exception vectors */
#define SCTLR_V      (1 << 13)
/** Round Robin */
#define SCTLR_RR     (1 << 14)
/** Hardware Access Flag Enable */
#define SCTLR_HA     (1 << 17)
/** Fast Interupts configuration enable */
#define SCTLR_FI     (1 << 21)
/** Interrupt Vectors Enable */
#define SCTLR_VE     (1 << 24)
/** Exception Endianness */
#define SCTLR_EE     (1 << 25)
/** Non-maskable Fast Interrupts enable */
#define SCTLR_NMFI   (1 << 27)
/** TX Remap Enable */
#define SCTLR_TRE    (1 << 28)
/** Access Flag Enable*/
#define SCTLR_AFE    (1 << 29)
/** Thumb Exception enable */
#define SCTLR_TE     (1 << 30)

/** Access Rights mask */
#define CPAC_MASK         0x3
/** Access denied */
#define CPAC_DENIED       0x0  
/** Privileged access only */
#define CPAC_PL1          0x1 
/** Full access */
#define CPAC_FULL         0x3

/** Access rights for coprocessor n */
#define CPACR_CPN(n, a)   ((a) << (2 * n))

/** Enable */
#define FPEXC_EN          (1 << 30)
/** Exception */
#define FPEXC_EX          (1 << 31)

/** Domain access permissions bitmask */
#define DA_MASK         0x3
/** No access */
#define DA_NO           0x0
/** Client */
#define DA_CLIENT       0x1
/** Manager */
#define DA_MANAGER      0x3

/** Domain n access permission bits */
#define DACR_DN(n, x)   ((x) << (n * 2))

/** Cortex-A9 MPCore CPU ID */
#define MPIDR_CPU_ID    3

#ifndef __ASSEMBLER__

#include <stdint.h>

/**
 * Get the value of the CPSR register.
 *
 * @return The value of the CPSR register.
 */
static inline uint32_t
cpsr_get(void)
{
  uint32_t val;

  asm volatile ("mrs %0, cpsr" : "=r" (val));
  return val;
}

/**
 * Set the value of the CPSR register.
 *
 * @param val The value to be set.
 */
static inline void
cpsr_set(uint32_t val)
{
  asm volatile ("msr cpsr, %0" : : "r" (val));
}

// Helper function to stringize macros
#define __XSTR(s...) #s

// Define a getter function for a CP15 register
#define CP15_GETTER(name, arg)                        \
  static inline uint32_t                              \
  name(void)                                          \
  {                                                   \
    uint32_t val;                                     \
    asm volatile ("mrc " __XSTR(arg) : "=r" (val));   \
    return val;                                       \
  }

// Define a setter function for a CP15 register
#define CP15_SETTER(name, arg)                        \
  static inline void                                  \
  name(uint32_t val)                                  \
  {                                                   \
    asm volatile ("mcr " __XSTR(arg) : : "r" (val));  \
  }

CP15_GETTER(cp15_mpidr_get, CP15_MPIDR(%0));
CP15_GETTER(cp15_sctlr_get, CP15_SCTLR(%0));
CP15_SETTER(cp15_sctlr_set, CP15_SCTLR(%0));
CP15_SETTER(cp15_ttbr0_set, CP15_TTBR0(%0));
CP15_SETTER(cp15_ttbr1_set, CP15_TTBR1(%0));
CP15_SETTER(cp15_ttbcr_set, CP15_TTBCR(%0));
CP15_GETTER(cp15_dfsr_get, CP15_DFSR(%0));
CP15_GETTER(cp15_ifsr_get, CP15_IFSR(%0));
CP15_GETTER(cp15_dfar_get, CP15_DFAR(%0));
CP15_GETTER(cp15_ifar_get, CP15_IFAR(%0));

/**
 * Invalidate entire unified TLB.
 */
static inline void
cp15_tlbiall(void)
{
  asm volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r"(0));
}

/**
 * TLB Invalidate by MVA.
 */
static inline void
cp15_tlbimva(uintptr_t va)
{
  asm volatile ("mcr p15, 0, %0, c8, c7, 1" : : "r"(va));
}

/**
 * Get the value of the R11 (FP) register.
 *
 * @return The value of the R11 register.
 */
static inline uint32_t
r11_get(void)
{
  uint32_t val;

  asm volatile ("mov %0, r11\n" : "=r" (val)); 
  return val;
}

#endif  // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_ARCH_KERNEL_REGS_H__
