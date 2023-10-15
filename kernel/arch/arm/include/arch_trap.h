#ifndef __AG_KERNEL_ARCH_TRAP_H__
#define __AG_KERNEL_ARCH_TRAP_H__

/** Reset */
#define T_RESET     0
/** Undefined Instruction */
#define T_UNDEF     1
/** Supervisor Call (SVC) */
#define T_SWI       2
/** Prefetch Abort */
#define T_PABT      3
/** Data Abort */
#define T_DABT      4
/** Not Used */
#define T_UNUSED    5
/** IRQ (Interrupt) */
#define T_IRQ       6
/** FIQ (Fast Interrupt) */
#define T_FIQ       7

#ifndef __ASSEMBLER__

#include <stdint.h>

/** Generic Trap Frame */
struct TrapFrame {
  uint32_t  psr;
  uint32_t  trapno;
  uint32_t  sp;
  uint32_t  lr;
  uint32_t  r0;
  uint32_t  r1;
  uint32_t  r2;
  uint32_t  r3;
  uint32_t  r4;
  uint32_t  r5;
  uint32_t  r6;
  uint32_t  r7;
  uint32_t  r8;
  uint32_t  r9;
  uint32_t  r10;
  uint32_t  r11;
  uint32_t  r12;
  uint32_t  pc;
};

void arch_trap_init(void);
void arch_trap(struct TrapFrame *);
void arch_trap_print_frame(struct TrapFrame *);

#endif  // !__ASSEMBLER__

#endif  // __AG_KERNEL_ARCH_TRAP_H__
