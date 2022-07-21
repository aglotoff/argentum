#ifndef __KERNEL_ARMV7_H__
#define __KERNEL_ARMV7_H__

/**
 * @file kernel/include/armv7.h
 * 
 * This file contains definitions for core and system registers, the memory
 * management unit (MMU), and routines to let C code use special ARMv7
 * instructions.
 */

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

/**
 * Wait for Interrupt.
 */
static inline void
wfi(void)
{
  asm volatile("wfi");
}

#endif  // !__ASSEMBLER__

/** @defgroup AccessPermissions Access Permissions bits
 *  @{
 */
#define AP_MASK             0x23      ///< Access permissions bitmask
#define AP_PRIV_RW          0x01      ///<   Privileged access
#define AP_USER_RO          0x02      ///<   User read-only
#define AP_BOTH_RW          0x03      ///<   Full access
#define AP_PRIV_RO          0x21      ///<   Privileged read-only
#define AP_BOTH_RO          0x23      ///<   Privileged and user read-only
/** @} */

/** Offset of first-level table index in a virtual address */
#define L1_IDX_SHIFT        20
/** First-level table index */
#define L1_IDX(va)          (((uint32_t) (va) >> L1_IDX_SHIFT) & 0xFFF)
/** The number of entries in a first-level table */
#define L1_NR_ENTRIES       4096
/** Total size of a first-level table in bytes */
#define L1_TABLE_SIZE       (L1_NR_ENTRIES * 4)

/** Offset of second-level table index in a virtual address */
#define L2_IDX_SHIFT        12
/** Second-level table index */
#define L2_IDX(va)          (((uint32_t) (va) >> L2_IDX_SHIFT) & 0xFF)
/** The number of entries in a second-level table */
#define L2_NR_ENTRIES       256
/** Total size of a second-level table in bytes */
#define L2_TABLE_SIZE       (L2_NR_ENTRIES * 4)

/** The number of bytes mapped by a section */
#define L1_SECTION_SIZE     1232896
/** The number of bytes mapped by a small page */
#define L2_PAGE_SM_SIZE     4096
/** The number of bytes mapped by a large page */
#define L2_PAGE_LG_SIZE     65536

/** @defgroup L1DescBits First-level descriptor bits and fields
 *  @{
 */
#define L1_DESC_TYPE_MASK         0x3           ///< Descriptor type bitmask
#define L1_DESC_TYPE_FAULT        0x0           ///<   Invalid or fault entry
#define L1_DESC_TYPE_TABLE        0x1           ///<   Page table
#define L1_DESC_TYPE_SECT         0x2           ///<   Section or Supersection

#define L1_DESC_TABLE_NS          (1 << 3)      ///< Non-secure
#define L1_DESC_TABLE_DOMAIN(x)   ((x) << 5)    ///< Domain field
#define L1_DESC_TABLE_DOMAIN_MASK (0xF << 5)    ///< Domain field bitmask

#define L1_DESC_SECT_B            (1 << 2)      ///< Bufferable
#define L1_DESC_SECT_C            (1 << 3)      ///< Cacheable
#define L1_DESC_SECT_XN           (1 << 4)      ///< Execute-never
#define L1_DESC_SECT_DOMAIN(x)    ((x) << 5)    ///< Domain field
#define L1_DESC_SECT_DOMAIN_MASK  (0xF << 5)    ///< Domain field bitmask
#define L1_DESC_SECT_AP(x)        ((x) << 10)   ///< Access permissions bits
#define L1_DESC_SECT_TEX(x)       ((x) << 12)   ///< TEX remap bits
#define L1_DESC_SECT_S            (1 << 16)     ///< Shareable
#define L1_DESC_SECT_NG           (1 << 17)     ///< Not global
#define L1_DESC_SECT_SUPER        (1 << 18)     ///< Supersection
#define L1_DESC_SECT_NS           (1 << 19)     ///< Non-secure
/** @} */

/** Page table base address */
#define L1_DESC_TABLE_BASE(x)     ((x) & ~0x3FF)
/** Section base address */
#define L1_DESC_SECT_BASE(x)      ((x) & ~0xFFFFF)

/** @defgroup L2DescBits Second-level descriptor bits and fields
 *  @{
 */
#define L2_DESC_TYPE_MASK         0x3           ///< Descriptor type bitmask
#define L2_DESC_TYPE_FAULT        0x0           ///<   Invalid or fault entry
#define L2_DESC_TYPE_LG           0x1           ///<   Large page
#define L2_DESC_TYPE_SM           0x2           ///<   Small page

#define L2_DESC_B                 (1 << 2)      ///< Bufferable
#define L2_DESC_C                 (1 << 3)      ///< Cacheable
#define L2_DESC_AP(x)             ((x) << 4)    ///< Access permissions bits
#define L2_DESC_S                 (1 << 10)     ///< Shareable
#define L2_DESC_NG                (1 << 11)     ///< Not global

#define L2_DESC_LG_TEX(x)         ((x) << 12)   ///< TEX remap
#define L2_DESC_LG_XN             (1 << 15)     ///< Execute-never

#define L2_DESC_SM_XN             (1 << 0)      ///< Execute-never
#define L2_DESC_SM_TEX(x)         ((x) << 6)    ///< TEX remap
/** @} */

/** Large page base address */
#define L2_DESC_LG_BASE(x)        ((x) & ~0xFFFF)
/** Small page base address */
#define L2_DESC_SM_BASE(x)        ((x) & ~0xFFF)

#ifndef __ASSEMBLER__

/** First-level descriptor */
typedef uint32_t  l1_desc_t;
/** Second-level descriptor */
typedef uint32_t  l2_desc_t;

#endif  // !__ASSEMBLER__

#endif  // !__KERNEL_ARMV7_H__
