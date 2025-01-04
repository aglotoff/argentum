#ifndef __ARCH_I386_TRAP_H__
#define __ARCH_I386_TRAP_H__

#define T_DE      0     // Divide error
#define T_DB      1     // Reserved
#define T_NMI     2     // NMI Interrupt
#define T_BP      3     // Breakpoint
#define T_OF      4     // Overflow
#define T_BR      5     // BOUND Range Exceeded
#define T_UD      6     // Undefined Opcode
#define T_NM      7     // No Math Coprocessor
#define T_DF      8     // Double Fault
#define T_CSS     9     // Coprocessor Segment Overrun
#define T_TS      10    // Invalid TSS
#define T_NP      11    // Segment Not Present
#define T_SS      12    // Stack-Segment Fault
#define T_GP      13    // General Protection Fault
#define T_PF      14    // Page Fault
#define T_MF      16    // Math Fault
#define T_AC      17    // Alignment Check
#define T_MC      18    // Machine Check
#define T_XF      19    // SIMD Floating-Point Exception
#define T_IRQ0    32    // User Defined

#define IRQ_PIT       0
#define IRQ_KEYBOARD  1
#define IRQ_CASCADE   2
#define IRQ_COM2      3
#define IRQ_COM1      4
#define IRQ_LPT2      5
#define IRQ_FLOPPY    6
#define IRQ_LPT1      7
#define IRQ_CMOS      8
#define IRQ_MOUSE     12
#define IRQ_FPU       13
#define IRQ_ATA1      14
#define IRQ_ATA2      15

#ifndef __ASSEMBLER__

#include <stdint.h>

/** Generic Trap Frame */
struct TrapFrame {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t _esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;

  uint16_t gs;
  uint16_t padding1;
  uint16_t fs;
  uint16_t padding2;
  uint16_t es;
  uint16_t padding3;
  uint16_t ds;
  uint16_t padding4;

  uint32_t trapno;
  uint32_t error;
  uint32_t eip;
  uint16_t cs;
  uint16_t padding5;
  uint32_t eflags;

  uint32_t esp;
  uint16_t ss;
  uint16_t padding6;
};

void trap(struct TrapFrame *);
void print_trapframe(struct TrapFrame *);

#endif  // !__ASSEMBLER__

#endif  // !__ARCH_I386_TRAP_H__
