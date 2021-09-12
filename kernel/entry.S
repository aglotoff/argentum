#include "armv7.h"
#include "memlayout.h"

#define RELOC(x)  ((x) - KERNEL_BASE)

.section .entry, "a"

/*
 *
 * Vector Table
 *
 */

  .globl vectors
vectors:
  b       .                   // Reset
  ldr     pc, [pc, #0x18]     // Undefined Instruction
  b       .                   // Supervisor Call (SVC)
  ldr     pc, [pc, #0x18]     // Prefetch Abort
  ldr     pc, [pc, #0x18]     // Data Abort
  b       .                   // Not Used
  ldr     pc, [pc, #0x18]     // IRQ (interrupt)
  b       .                   // FIQ (fast interrupt)

  // Trap handler addresses
  .long   0
  .long   trap_undef
  .long   0
  .long   trap_pabt
  .long   trap_dabt
  .long   0
  .long   trap_irq
  .long   0

/*
 *
 * Kernel entry point
 *
 */

  .globl  _start
_start:
  // Load the physical address of the initial translation table
  ldr r0, =RELOC(entry_trtab)
  mcr p15, 0, r0, c2, c0, 0

  // Assign domain access
  ldr r0, =DACR_D(0, DA_CLIENT)
  mcr p15, 0, r0, c3, c0, 0

  // Enable cache and MMU
  mrc p15, 0, r0, c1, c0, 0
  ldr r1, =(SCTLR_M | SCTLR_C | SCTLR_I)
  orr r0, r0, r1
  mcr p15, 0, r0, c1, c0, 0

  // Set the kernel exception stack pointer
  ldr r0, =kxstack
  msr CPSR_c, #(PSR_I | PSR_F | PSR_M_IRQ)
  mov sp, r0
  msr CPSR_c, #(PSR_I | PSR_F | PSR_M_ABT)
  mov sp, r0
  msr CPSR_c, #(PSR_I | PSR_F | PSR_M_UND)
  mov sp, r0

  msr CPSR_c, #(PSR_I | PSR_F | PSR_M_SYS)
  mov sp, #0x123
  mov lr, #0x456

  // Set the kernel supervisor mode stack pointer
  msr CPSR_c, #(PSR_I | PSR_F | PSR_M_SVC)
  ldr sp, =kstack_top

  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly
  mov r11, #0

  // Jump to C code
  ldr r0, =main
  blx r0

  // Should never return here
1:
  b 1b

.bss

/*
 *
 * Kernel stack.
 *
 */
  .globl    kstack, kstack_top
  .p2align  12
kstack:
  .space  KSTACK_SIZE
kstack_top:

/*
 *
 * Kernel exception stack
 *
 */
  .globl    kxstack, kxstack_top
  .p2align  2
kxstack:
  .space  KXSTACK_SIZE
kxstack_top:
