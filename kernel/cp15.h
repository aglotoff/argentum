#ifndef KERNEL_CP15_H
#define KERNEL_CP15_H

/**
 * @file kernel/cp15.h
 * 
 * CP15 Registers Flags
 */

// System Control Register bits
#define SCTLR_M         (1 << 0)      ///< MMU enable
#define SCTLR_A         (1 << 1)      ///< Alignment
#define SCTLR_C         (1 << 2)      ///< Cache enable
#define SCTLR_SW        (1 << 10)     ///< SWP/SWPB Enable
#define SCTLR_Z         (1 << 11)     ///< Branch prediction enable
#define SCTLR_I         (1 << 12)     ///< Instruction cache enable
#define SCTLR_V         (1 << 13)     ///< High exception vectors
#define SCTLR_RR        (1 << 14)     ///< Round Robin
#define SCTLR_HA        (1 << 17)     ///< Hardware Access Flag Enable
#define SCTLR_FI        (1 << 21)     ///< Fast Interupts configuration enable
#define SCTLR_VE        (1 << 24)     ///< Interrupt Vectors Enable
#define SCTLR_EE        (1 << 25)     ///< Exception Endianness
#define SCTLR_NMFI      (1 << 27)     ///< Non-maskable Fast Interrupts enable
#define SCTLR_TRE       (1 << 28)     ///< TX Remap Enable
#define SCTLR_AFE       (1 << 29)     ///< Acces Flag Enable
#define SCTLR_TE        (1 << 30)     ///< Thumb Exception enable

// Domain access permission bits
#define DA_MASK         0x3           ///< Domain access permissions bitmask
#define DA_NO           0x0           ///<   No access
#define DA_CLIENT       0x1           ///<   Client
#define DA_MANAGER      0x3           ///<   Manager

/** Domain n access permission bits */
#define DACR_D(n, x)    ((x) << (n * 2))

#endif  // !KERNEL_CP15_H
