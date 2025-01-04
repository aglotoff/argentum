#ifndef __ARCH_I386_REGS_H__
#define __ARCH_I386_REGS_H__

#define CR0_PE          (1 << 0)    // Protection Enable
#define CR0_WP          (1 << 16)   // Write Protect
#define CR0_PG          (1 << 31)   // Paging

#define CR4_PSE         (1 << 4)    // Page Size Extensions

#define EFLAGS_IF       (1 << 9)    // Interrupt enable

#ifndef __ASSEMBLER__

#include <stdint.h>

static inline uint32_t
cr2_get(void)
{
  uint32_t value;

  asm volatile("movl %%cr2, %0" : "=r" (value));
  return value;
}

static inline void
cr3_set(uint32_t value)
{
  asm volatile("movl %0, %%cr3" : : "r" (value));
}

static inline uint32_t
eflags_get(void)
{
  uint32_t eflags;

  asm volatile("pushfl; popl %0" : "=r" (eflags));
  return eflags;
}

static inline uint32_t
ebp_get(void)
{
	uint32_t ebp;

	asm volatile("movl %%ebp,%0" : "=r" (ebp));
	return ebp;
}

#endif  // !__ASSEMBLER__

#endif  // !__ARCH_I386_REGS_H__
