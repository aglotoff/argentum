#ifndef __ARCH_I386_REGS_H__
#define __ARCH_I386_REGS_H__

#define CR0_PE          (1 << 0)    // Protection Enable
#define CR0_WP          (1 << 16)   // Write Protect
#define CR0_PG          (1 << 31)   // Paging

#define CR4_PSE         (1 << 4)    // Page Size Extensions

#ifndef __ASSEMBLER__

#include <stdint.h>

static inline void
lcr3_set(uint32_t value)
{
  asm volatile("movl %0, %%cr3" : : "r" (value));
}

#endif  // !__ASSEMBLER__

#endif  // !__ARCH_I386_REGS_H__
