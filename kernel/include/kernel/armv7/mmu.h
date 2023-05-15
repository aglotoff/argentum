#ifndef __KERNEL_INCLUDE_KERNEL_ARMV7_MMU_H__
#define __KERNEL_INCLUDE_KERNEL_ARMV7_MMU_H__

/**
 * @file include/argentum/armv7/regs.h
 * 
 * This file contains definitions for the ARMv7 Memory Management Unit (MMU).
 */

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

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

#include <stdint.h>

/** First-level descriptor */
typedef uint32_t  l1_desc_t;
/** Second-level descriptor */
typedef uint32_t  l2_desc_t;

#endif  // !__ASSEMBLER__

#endif  // !__KERNEL_INCLUDE_KERNEL_ARMV7_MMU_H__
