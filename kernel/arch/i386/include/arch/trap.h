#ifndef __ARCH_I386_TRAP_H__
#define __ARCH_I386_TRAP_H__

#ifndef __ASSEMBLER__

#include <stdint.h>

/** Generic Trap Frame */
struct TrapFrame {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t oesp;
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

  uint32_t err;
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
