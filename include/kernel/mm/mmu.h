#ifndef __KERNEL_MM_MMU_H__
#define __KERNEL_MM_MMU_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/mmu.h
 * 
 * Definitions for the ARMv7 memory-management unit (MMU).
 */

#define TTX_SHIFT           20        ///< Offset of TTX in a virtual address
#define PTX_SHIFT           12        ///< Offset of PTX in a virtual address

/** Translation table index */
#define TTX(va)             (((uint32_t) (va) >> TTX_SHIFT) & 0xFFF)

/** Page table index */
#define PTX(va)             (((uint32_t) (va) >> PTX_SHIFT) & 0xFF)

#define PAGE_SMALL_SIZE     4096      ///< Bytes mapped by a small page
#define PAGE_LARGE_SIZE     65536     ///< Bytes mapped by a large page
#define PAGE_SECT_SIZE      1232896   ///< Bytes mapped by a section

#define NTTENTRIES          4096      ///< Entries per translation table
#define NPTENTRIES          256       ///< Entries per page table

#define TRTAB_SIZE          (NTTENTRIES * 4)  ///< Translation table size
#define PGTAB_SIZE          (NPTENTRIES * 4)  ///< Page table size

// Access permissions bits
#define AP_MASK             0x23      ///< Access permissions bitmask
#define AP_PRIV_RW          0x01      ///<   Privileged access
#define AP_USER_RO          0x02      ///<   User read-only
#define AP_BOTH_RW          0x03      ///<   Full access
#define AP_PRIV_RO          0x21      ///<   Privileged read-only
#define AP_BOTH_RO          0x23      ///<   Privileged and user read-only

// Translation table level 1 descriptor bits
#define TTE_TYPE_MASK       0x3           ///< Descriptor type bitmask
#define TTE_TYPE_FAULT      0x0           ///<   Invalid or fault entry
#define TTE_TYPE_PGTAB      0x1           ///<   Page table
#define TTE_TYPE_SECT       0x2           ///<   Section or Supersection
#define TTE_DOMAIN(x)       ((x) << 5)    ///< Domain field
#define TTE_DOMAIN_MASK     (0xF << 5)    ///< Domain field bitmask

// Page table descriptor bits
#define TTE_PGTAB_NS        (1 << 3)      ///< Non-secure

/** Base address in Page table descriptor */
#define TTE_PGTAB_ADDR(tte)  ((uint32_t) tte & ~0x3FF)

// Section or Supersection descriptor bits
#define TTE_SECT_B          (1 << 2)      ///< Bufferable
#define TTE_SECT_C          (1 << 3)      ///< Cacheable
#define TTE_SECT_XN         (1 << 4)      ///< Execute-never
#define TTE_SECT_AP(x)      ((x) << 10)   ///< Access permissions bits
#define TTE_SECT_TEX(x)     ((x) << 12)   ///< TEX remap bits
#define TTE_SECT_S          (1 << 16)     ///< Shareable
#define TTE_SECT_NG         (1 << 17)     ///< Not global
#define TTE_SECT_SUPER      (1 << 18)     ///< Supersection
#define TTE_SECT_NS         (1 << 19)     ///< Non-secure

/** Base address in Section or Supersection descriptor */
#define TTE_SECT_ADDR(tte)  ((uint32_t) tte & ~0xFFFFF)

// Translation table level 2 descriptor bits
#define PTE_TYPE_MASK       0x3           ///< Descriptor type bitmask
#define PTE_TYPE_FAULT      0x0           ///<   Invalid or fault entry
#define PTE_TYPE_LARGE      0x1           ///<   Large page
#define PTE_TYPE_SMALL      0x2           ///<   Small page
#define PTE_B               (1 << 2)      ///< Bufferable
#define PTE_C               (1 << 3)      ///< Cacheable
#define PTE_AP(x)           ((x) << 4)    ///< Access permissions bits
#define PTE_S               (1 << 10)     ///< Shareable
#define PTE_NG              (1 << 11)     ///< Not global

// Large page descriptor bits
#define PTE_LARGE_TEX(x)    ((x) << 12)   ///< TEX remap
#define PTE_LARGE_XN        (1 << 15)     ///< Execute-never

/** Base address in Large page descriptor */
#define PTE_LARGE_ADDR(pte) ((pte) & ~0xFFFF)

// Small page descriptor bits
#define PTE_SMALL_XN        (1 << 0)      ///< Execute-never
#define PTE_SMALL_TEX(x)    ((x) << 6)    ///< TEX remap

/** Base address in Small page descriptor */
#define PTE_SMALL_ADDR(pte) ((pte) & ~0xFFF)

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef uint32_t  tte_t;
typedef uint32_t  pte_t;

#endif  // !__ASSEMBLER__

#endif  // !__KERNEL_MMU_H__
