#include <kernel/armv7.h>
#include <kernel/mm/memlayout.h>

#define RELOC(x)  ((x) - KERNEL_BASE)

.section .entry, "a"

/*
 * ----------------------------------------------------------------------------
 * Vector Table
 * ----------------------------------------------------------------------------
 */

  .globl _start
_start:
  b       entry               // Reset
  ldr     pc, [pc, #0x18]     // Undefined Instruction
  ldr     pc, [pc, #0x18]     // Supervisor Call (SVC)
  ldr     pc, [pc, #0x18]     // Prefetch Abort
  ldr     pc, [pc, #0x18]     // Data Abort
  b       .                   // Not Used
  ldr     pc, [pc, #0x18]     // IRQ (interrupt)
  b       .                   // FIQ (fast interrupt)

  // Trap handler addresses
  .long   0
  .long   trap_undef
  .long   trap_svc
  .long   trap_pabt
  .long   trap_dabt
  .long   0
  .long   trap_irq
  .long   0

/*
 * ----------------------------------------------------------------------------
 * Kernel entry point
 * ----------------------------------------------------------------------------
 *
 * Each CPU starts executing here.
 *
 */

  .globl  entry
entry:
  // Load the physical address of the initial translation table
  ldr   r0, =RELOC(entry_trtab)
  mcr   p15, 0, r0, c2, c0, 0

  // Assign domain access
  ldr   r0, =DACR_D(0, DA_CLIENT)
  mcr   p15, 0, r0, c3, c0, 0

  // Enable MMU, cache, and high exception vectors
  mrc   p15, 0, r0, c1, c0, 0
  ldr   r1, =(SCTLR_M | SCTLR_C | SCTLR_I | SCTLR_V)
  orr   r0, r0, r1
  mcr   p15, 0, r0, c1, c0, 0

  // Get the current processor ID
  mrc   p15, 0, r0, c0, c0, 5
  and   r0, r0, #3

  // Set the kernel exception stack pointer for this CPU
  // sp = kxstack + (KXSTACK_SIZE * <cpu_id>)
  ldr   r2, =kxstack
  mov   r1, #KXSTACK_SIZE
  mul   r1, r0
  add   r1, r2

  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_IRQ)  // IRQ mode
  mov   sp, r1
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_ABT)  // Abort mode
  mov   sp, r1
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_UND)  // Undef mode
  mov   sp, r1

  // Set the kernel supervisor mode stack pointer for this CPU
  // sp = kstack_top - (KSTACK_SIZE * <cpu_id>)
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_SVC)
  ldr   sp, =kstack_top
  mov   r1, #KSTACK_SIZE
  mul   r1, r0
  sub   sp, r1

  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov   r11, #0

  // BSP calls main(), APs call mp_enter().
  cmp   r0, #0
  ldreq r0, =main
  ldrne r0, =mp_enter
  blx   r0

  // Should never return here
  b     .

.bss

/*
 * ----------------------------------------------------------------------------
 * Per-CPU kernel stacks
 * ----------------------------------------------------------------------------
 *
 * Each CPU has its own scheduler thread and thus needs a separate supervisor
 * mode stack.
 *
 */

  .globl    kstack, kstack_top
  .p2align  12
kstack:
  .space    (KSTACK_SIZE * 4)
kstack_top:

/*
 * ----------------------------------------------------------------------------
 * Per-CPU kernel exception stacks
 * ----------------------------------------------------------------------------
 */
  .globl    kxstack, kxstack_top
  .p2align  2
kxstack:
  .space    (KXSTACK_SIZE * 4)
kxstack_top:
