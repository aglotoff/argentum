#include <arch_vm.h>
#include <smp.h>
#include <memlayout.h>
#include <regs.h>

.section .entry, "a"

/*
 * ----------------------------------------------------------------------------
 * Vector Table
 * ----------------------------------------------------------------------------
 *
 */

  .globl vectors, vectors_end
vectors:
  ldr     pc, [pc, #0x18]     // Reset
  ldr     pc, [pc, #0x18]     // Undefined Instruction
  ldr     pc, [pc, #0x18]     // Supervisor Call (SVC)
  ldr     pc, [pc, #0x18]     // Prefetch Abort
  ldr     pc, [pc, #0x18]     // Data Abort
  b       .                   // Not Used
  ldr     pc, [pc, #0x18]     // IRQ (interrupt)
  b       .                   // FIQ (fast interrupt)

  // Trap handler addresses
  .long   _start
  .long   trap_undef
  .long   trap_svc
  .long   trap_pabt
  .long   trap_dabt
  .long   0
  .long   trap_irq
  .long   0
vectors_end:

/*
 * ----------------------------------------------------------------------------
 * Kernel entry point
 * ----------------------------------------------------------------------------
 *
 * Each CPU starts executing here.
 *
 */

  .globl _start
_start:
  // Set access rights to CP10 and CP11 (the FPU coprocessors)
  ldr   r0, =(CPACR_CPN(10, CPAC_FULL) | CPACR_CPN(11, CPAC_FULL))
  mcr   CP15_CPACR(r0)

  // Enable the FPU
  mov   r3, #FPEXC_EN 
  vmsr  fpexc, r3

  // Load the physical address of the initial translation table
  ldr   r2, =(entry_pgdir - VIRT_KERNEL_BASE)
  mcr   CP15_TTBR0(r2)

  // Assign domain access
  ldr   r2, =DACR_DN(0, DA_CLIENT)
  mcr   CP15_DACR(r2)

  // Enable MMU, cache, and high exception vectors
  mrc   CP15_SCTLR(r2)
  ldr   r1, =(SCTLR_M | SCTLR_C | SCTLR_I | SCTLR_V)
  orr   r2, r2, r1
  mcr   CP15_SCTLR(r2)

  // Get the current processor ID
  mrc   CP15_MPIDR(r0)
  and   r0, r0, #3

  // Set the kernel exception stack pointer for this CPU
  // sp = xstack + (XSTACK_SIZE * <cpu_id>)
  ldr   fp, =xstack
  mov   r1, #XSTACK_SIZE
  mul   r1, r0
  add   fp, r1

  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_IRQ)  // IRQ mode
  mov   sp, fp
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_ABT)  // Abort mode
  mov   sp, fp
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_UND)  // Undef mode
  mov   sp, fp

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

  cmp   r0, #0
  ldrne r1, =arch_smp_main
  ldreq r1, =main
  blx   r1
  b     .

.section .bss

/*
 * ----------------------------------------------------------------------------
 * Per-CPU kernel stacks
 * ----------------------------------------------------------------------------
 *
 * Each CPU has its own scheduler pseudo-task and thus needs a separate
 * supervisor mode stack.
 *
 */

  .globl    kstack, kstack_top
  .p2align  12
kstack:
  .space    (KSTACK_SIZE * SMP_CPU_MAX)
kstack_top:

/*
 * ----------------------------------------------------------------------------
 * Per-CPU exception stacks
 * ----------------------------------------------------------------------------
 */

  .globl    xstack, xstack_top
  .p2align  2
xstack:
  .space    (XSTACK_SIZE * SMP_CPU_MAX)
xstack_top:
