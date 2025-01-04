#ifndef __ARCH_I386_MMU_H__
#define __ARCH_I386_MMU_H__

#define PGTAB_NR_ENTRIES  1024

#define PGDIR_NR_ENTRIES  1024
#define PGDIR_SIZE        (PGDIR_NR_ENTRIES * 4)

#define PDE_P             (1 << 0)  // Present
#define PDE_W             (1 << 1)  // Write
#define PDE_U             (1 << 2)  // User
#define PDE_PWT           (1 << 3)  // Write-Through
#define PDE_PCD           (1 << 4)  // Cache-Disabled
#define PDE_A             (1 << 5)  // Accessed
#define PDE_D             (1 << 6)  // Dirty
#define PDE_PS            (1 << 7)  // Page Size
#define PDE_G             (1 << 8)  // Global

#define PTE_P             (1 << 0)  // Present
#define PTE_W             (1 << 1)  // Write
#define PTE_U             (1 << 2)  // User
#define PTE_PWT           (1 << 3)  // Write-Through
#define PTE_PCD           (1 << 4)  // Cache-Disabled
#define PTE_A             (1 << 5)  // Accessed
#define PTE_D             (1 << 6)  // Dirty

#define PTE_AVAIL_COW     (1 << 9)
#define PTE_AVAIL_PAGE    (1 << 10)

#define PTE_FLAGS(x)      ((x) & 0xFFF)
#define PTE_BASE(x)       ((x) & ~0xFFF)

#define PDE_FLAGS(x)      ((x) & 0xFFF)
#define PDE_BASE(x)       ((x) & ~0xFFF)

#define LARGE_PAGE_SIZE   (PAGE_SIZE * PGDIR_NR_ENTRIES)

#define PL_MASK         3
#define PL_KERNEL       0
#define PL_USER         3

#define GD_NULL         0
#define GD_KERNEL_CODE  1
#define GD_KERNEL_DATA  2
#define GD_USER_CODE    3
#define GD_USER_DATA    4
#define GD_TSS          5

#define SEG_KERNEL_CODE ((GD_KERNEL_CODE << 3) | PL_KERNEL)
#define SEG_KERNEL_DATA ((GD_KERNEL_DATA << 3) | PL_KERNEL)
#define SEG_USER_CODE   ((GD_USER_CODE << 3) | PL_USER)
#define SEG_USER_DATA   ((GD_USER_DATA << 3) | PL_USER)
#define SEG_TSS         ((GD_TSS << 3) | PL_KERNEL)

#ifndef __ASSEMBLER__

#include <stdint.h>

#define PGDIR_IDX_SHIFT   22
#define PGDIR_IDX(va)     (((uint32_t) (va) >> PGDIR_IDX_SHIFT) & 0x3FF)

#define PGTAB_IDX_SHIFT   12
#define PGTAB_IDX(va)     (((uint32_t) (va) >> PGTAB_IDX_SHIFT) & 0x3FF)

typedef uint32_t  pde_t;
typedef uint32_t  pte_t;

struct SegDesc {
  unsigned limit_15_00 : 16;
  unsigned base_15_00  : 16;
  unsigned base_23_16  : 8;
  unsigned type        : 4;
  unsigned s           : 1;
  unsigned dpl         : 2;
  unsigned p           : 1;
  unsigned limit_19_16 : 4;
  unsigned avl         : 1;
  unsigned zero        : 1;
  unsigned db          : 1;
  unsigned g           : 1;
  unsigned base_31_24  : 8;
} __attribute__((__packed__));

struct PseudoDesc {
  uint16_t limit;
  uint32_t base;
} __attribute__((__packed__));

#define SEG_DESC_NULL  ((struct SegDesc) { \
  .limit_15_00 = 0,                   \
})

#define SEG_DESC_32(base, limit, t, d) ((struct SegDesc) {  \
  .limit_15_00 = (((size_t) (limit)) >> 12) & 0xFFFF,       \
  .base_15_00  = ((uintptr_t) (base)) & 0xFFFF,             \
  .base_23_16  = (((uintptr_t) (base)) >> 16) & 0xFF,       \
  .type        = (t) & 0xF,                                 \
  .s           = 1,                                         \
  .dpl         = (d) & 0x3,                                 \
  .p           = 1,                                         \
  .limit_19_16 = (((size_t) (limit)) >> 28) & 0xF,          \
  .avl         = 0,                                         \
  .zero        = 0,                                         \
  .db          = 1,                                         \
  .g           = 1,                                         \
  .base_31_24  = (((uintptr_t) (base)) >> 24) & 0xFF        \
})

#define SEG_DESC_16(base, limit, t, d) ((struct SegDesc) {  \
  .limit_15_00 = ((size_t) (limit)) & 0xFFFF,       \
  .base_15_00  = ((uintptr_t) (base)) & 0xFFFF,             \
  .base_23_16  = (((uintptr_t) (base)) >> 16) & 0xFF,       \
  .type        = (t) & 0xF,                                 \
  .s           = 0,                                         \
  .dpl         = (d) & 0x3,                                 \
  .p           = 1,                                         \
  .limit_19_16 = (((size_t) (limit)) >> 16) & 0xF,          \
  .avl         = 0,                                         \
  .zero        = 0,                                         \
  .db          = 1,                                         \
  .g           = 0,                                         \
  .base_31_24  = (((uintptr_t) (base)) >> 24) & 0xFF        \
})

#define SEG_TYPE_DATA   (0 << 3)    // Data
#define SEG_TYPE_CODE   (1 << 3)    // Code
#define SEG_TYPE_E      (1 << 2)    // Expand-down
#define SEG_TYPE_C      (1 << 2)    // Conforming
#define SEG_TYPE_W      (1 << 1)    // Write
#define SEG_TYPE_R      (1 << 1)    // Read
#define SEG_TYPE_A      (1 << 0)    // Accessed 

static inline void
lgdt(void *p)
{
	asm volatile("lgdt (%0)" : : "r" (p));
}

static inline void
lidt(void *p)
{
	asm volatile("lidt (%0)" : : "r" (p));
}

struct IDTGate {
  unsigned offset_15_00 : 16;
  unsigned selector     : 16;
  unsigned zero         : 8;
  unsigned type         : 4;
  unsigned s            : 1;
  unsigned dpl          : 2;
  unsigned p            : 1;
  unsigned offset_31_16 : 16;
} __attribute__((__packed__));

#define SEG_TYPE_TSS16A   1
#define SEG_TYPE_LDT      2
#define SEG_TYPE_TSS16B   3
#define SEG_TYPE_CG16     4
#define SEG_TYPE_TG       5
#define SEG_TYPE_IG16     6
#define SEG_TYPE_TG16     7
#define SEG_TYPE_TSS32A   9
#define SEG_TYPE_TSS32B   11
#define SEG_TYPE_CG32     12
#define SEG_TYPE_IG32     14
#define SEG_TYPE_TG32     15

#define IDT_GATE(t, off, sel, d) ((struct IDTGate) {    \
  .offset_15_00 = ((uintptr_t) (off)) & 0xFFFF,         \
  .selector     = (sel),                                \
  .zero         = 0,                                    \
  .type         = (t),                                  \
  .s            = 0,                                    \
  .dpl          = (d),                                  \
  .p            = 1,                                    \
  .offset_31_16 = (((uintptr_t) (off)) >> 16) & 0xFFFF  \
})

struct TaskState {
	uint32_t link;	// Old ts selector
	uintptr_t esp0;	// Stack pointers and segment selectors
	uint16_t ss0;	//   after an increase in privilege level
	uint16_t padding1;
	uintptr_t esp1;
	uint16_t ss1;
	uint16_t padding2;
	uintptr_t esp2;
	uint16_t ss2;
	uint16_t padding3;
	uint32_t cr3;	// Page directory base
	uintptr_t eip;	// Saved state from last task switch
	uint32_t eflags;
	uint32_t eax;	// More saved state (registers)
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uintptr_t esp;
	uintptr_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint16_t es;		// Even more saved state (segment selectors)
	uint16_t padding4;
	uint16_t cs;
	uint16_t padding5;
	uint16_t ss;
	uint16_t padding6;
	uint16_t ds;
	uint16_t padding7;
	uint16_t fs;
	uint16_t padding8;
	uint16_t gs;
	uint16_t padding9;
	uint16_t ldt;
	uint16_t padding10;
	uint16_t t;		// Trap on task switch
	uint16_t iomb;	// I/O map base address
};

static inline void
ltr(uint16_t sel)
{
	asm volatile("ltr %0" : : "r" (sel));
}

#endif  // !__ASSEMBLER__

#endif  // !__ARCH_I386_MMU_H__
