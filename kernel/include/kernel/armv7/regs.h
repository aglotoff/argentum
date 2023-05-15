#ifndef __KERNEL_INCLUDE_KERNEL_ARMV7_REGS_H__
#define __KERNEL_INCLUDE_KERNEL_ARMV7_REGS_H__

/**
 * @file kernel/include/armv7/regs.h
 * 
 * This file contains definitions for core and system registers as well as 
 * routines to let C code access those registers.
 */

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/** @defgroup PsrBits Program Status Register bits
 *  @{
 */
#define PSR_M_MASK      0x1F          ///< Mode field bitmask
#define PSR_M_USR       0x10          ///<   User
#define PSR_M_FIQ       0x11          ///<   FIQ
#define PSR_M_IRQ       0x12          ///<   IRQ
#define PSR_M_SVC       0x13          ///<   Supervicor
#define PSR_M_MON       0x16          ///<   Monitor
#define PSR_M_ABT       0x17          ///<   Abort
#define PSR_M_UND       0x1B          ///<   Undefined
#define PSR_M_SYS       0x1F          ///<   System
#define PSR_T           (1 << 5)      ///< Thumb execution state bit
#define PSR_F           (1 << 6)      ///< Fast interrupt disable bit
#define PSR_I           (1 << 7)      ///< Interrupt disable bit
#define PSR_A           (1 << 8)      ///< Asynchronous abort disable bit
#define PSR_E           (1 << 9)      ///< Endianness execution state bit
#define PSR_GE_MASK     (0xF << 16)   ///< Greater than or Equal flags bitmask
#define PSR_J           (1 << 24)     ///< Jazelle bit
#define PSR_Q           (1 << 27)     ///< Cumulative saturation flag
#define PSR_V           (1 << 28)     ///< Overflow condition code flag
#define PSR_C           (1 << 29)     ///< Carry condition code flag
#define PSR_Z           (1 << 30)     ///< Zero condition code flag
#define PSR_N           (1 << 31)     ///< Negative condition code flag
/** @} */

/** @defgroup CP15Regs System Control Registers
 *  @{
 */
#define CP15_MPIDR(x)   p15, 0, x, c0, c0, 5  ///< Multiprocessor Affinity
#define CP15_SCTLR(x)   p15, 0, x, c1, c0, 0  ///< System Control
#define CP15_CPACR(x)   p15, 0, x, c1, c0, 2  ///< Coprocessor Access Control
#define CP15_TTBR0(x)   p15, 0, x, c2, c0, 0  ///< Translation Table Base 0
#define CP15_TTBR1(x)   p15, 0, x, c2, c0, 1  ///< Translation Table Base 1
#define CP15_TTBCR(x)   p15, 0, x, c2, c0, 2  ///< Translation Table Base Ctrl
#define CP15_DFSR(x)    p15, 0, x, c5, c0, 0  ///< Data Fault Status
#define CP15_IFSR(x)    p15, 0, x, c5, c0, 1  ///< Instruction Fault Status
#define CP15_DFAR(x)    p15, 0, x, c6, c0, 0  ///< Data Fault Address
#define CP15_IFAR(x)    p15, 0, x, c6, c0, 1  ///< Instruction Fault Address
#define CP15_DACR(x)    p15, 0, x, c3, c0, 0  ///< Domain Access Control
/** @} */

/** @defgroup SctlrBits System Control Register bits
 *  @{
 */
#define CP15_SCTLR_M      (1 << 0)    ///< MMU enable
#define CP15_SCTLR_A      (1 << 1)    ///< Alignment
#define CP15_SCTLR_C      (1 << 2)    ///< Cache enable
#define CP15_SCTLR_SW     (1 << 10)   ///< SWP/SWPB Enable
#define CP15_SCTLR_Z      (1 << 11)   ///< Branch prediction enable
#define CP15_SCTLR_I      (1 << 12)   ///< Instruction cache enable
#define CP15_SCTLR_V      (1 << 13)   ///< High exception vectors
#define CP15_SCTLR_RR     (1 << 14)   ///< Round Robin
#define CP15_SCTLR_HA     (1 << 17)   ///< Hardware Access Flag Enable
#define CP15_SCTLR_FI     (1 << 21)   ///< Fast Interupts configuration enable
#define CP15_SCTLR_VE     (1 << 24)   ///< Interrupt Vectors Enable
#define CP15_SCTLR_EE     (1 << 25)   ///< Exception Endianness
#define CP15_SCTLR_NMFI   (1 << 27)   ///< Non-maskable Fast Interrupts enable
#define CP15_SCTLR_TRE    (1 << 28)   ///< TX Remap Enable
#define CP15_SCTLR_AFE    (1 << 29)   ///< Access Flag Enable
#define CP15_SCTLR_TE     (1 << 30)   ///< Thumb Exception enable
/** @} */

/** @defgroup CPAccessRights Coprocessor Access Rights
 *  @{
 */
#define CPAC_MASK         0x3   ///< Access Rights mask
#define CPAC_DENIED       0x0   ///<   Access denied
#define CPAC_PL1          0x1   ///<   Privileged access only
#define CPAC_FULL         0x3   ///<   Full access
/** @} */

/** Access rights for coprocessor n */
#define CP15_CPACR_CPN(n, a)  ((a) << (2 * n))

/** @defgroup FpexcBits Floating-Point Exception Control register bits
 *  @{
 */
#define FPEXC_EN          (1 << 30)   ///< Enable
#define FPEXC_EX          (1 << 31)   ///< Wxception
/** @} */

/** @defgroup DaBits Domain Access Values
 *  @{
 */
#define DA_MASK         0x3           ///< Domain access permissions bitmask
#define DA_NO           0x0           ///<   No access
#define DA_CLIENT       0x1           ///<   Client
#define DA_MANAGER      0x3           ///<   Manager
/** @} */

/** Domain n access permission bits */
#define CP15_DACR_DN(n, x)  ((x) << (n * 2))

/** Cortex-A9 MPCore CPU ID */
#define CP15_MPIDR_CPU_ID   3

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

// Indices for the stack backtrace data structure represented as an array of
// uint32_t values. See ARM Procedure Call Standard for more details
// To generate this structure for all functions, the code needs to be compiled
// with the -mapcs-frame and -fno-omit-frame-pointer flags

/** Save code pointer (fp points here) */
#define APCS_FRAME_PC    0
/** Return link value */
#define APCS_FRAME_LINK  -1
/** Return sp value */
#define APCS_FRAME_SP    -2
/** Return fp value */
#define APCS_FRAME_FP    -3

#endif  // !__KERNEL_INCLUDE_KERNEL_ARMV7_REGS_H__
