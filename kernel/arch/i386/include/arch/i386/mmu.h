#ifndef __ARCH_I386_MMU_H__
#define __ARCH_I386_MMU_H__

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

#ifndef __ASSEMBLER__

#include <stdint.h>

#define PGDIR_IDX_SHIFT   22
#define PGDIR_IDX(va)     (((uint32_t) (va) >> PGDIR_IDX_SHIFT) & 0x3FF)

typedef uint32_t  pde_t;
typedef uint32_t  pte_t;

#endif  // !__ASSEMBLER__

#endif  // !__ARCH_I386_MMU_H__
