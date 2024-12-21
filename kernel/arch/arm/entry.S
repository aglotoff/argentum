#include <kernel/mm/memlayout.h>
#include <arch/arm/regs.h>

.section .entry, "a"

/*
 * ----------------------------------------------------------------------------
 * Vector Table
 * ----------------------------------------------------------------------------
 *
 */

  .globl _start
_start:
  ldr     pc, [pc, #0x18]     // Reset
  ldr     pc, [pc, #0x18]     // Undefined Instruction
  ldr     pc, [pc, #0x18]     // Supervisor Call (SVC)
  ldr     pc, [pc, #0x18]     // Prefetch Abort
  ldr     pc, [pc, #0x18]     // Data Abort
  b       .                   // Not Used
  ldr     pc, [pc, #0x18]     // IRQ (interrupt)
  b       .                   // FIQ (fast interrupt)

  // Trap handler addresses
  .long   entry
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
  mov   r4, r0
  mov   r5, r1

  // Set access rights to CP10 and CP11 (the FPU coprocessors)
  ldr   r0, =(CP15_CPACR_CPN(10, CPAC_FULL) | CP15_CPACR_CPN(11, CPAC_FULL))
  mcr   CP15_CPACR(r0)

  // Enable the FPU
  mov   r3, #FPEXC_EN 
  vmsr  fpexc, r3

  // Load the physical address of the initial translation table
  ldr   r2, =KVA2PA(entry_pgdir)
  mcr   CP15_TTBR0(r2)

  // Assign domain access
  ldr   r2, =CP15_DACR_DN(0, DA_CLIENT)
  mcr   CP15_DACR(r2)

  // Enable MMU, cache, and high exception vectors
  mrc   CP15_SCTLR(r2)
  ldr   r1, =(CP15_SCTLR_M | CP15_SCTLR_C | CP15_SCTLR_I | CP15_SCTLR_V)
  orr   r2, r2, r1
  mcr   CP15_SCTLR(r2)

  // Get the current processor ID
  mrc   CP15_MPIDR(r0)
  and   r0, r0, #3

  // Set the kernel exception stack pointer for this CPU
  // sp = kxstack + (KXSTACK_SIZE * <k_cpu_id>)
  ldr   fp, =kxstack
  mov   r1, #KXSTACK_SIZE
  mul   r1, r0
  add   fp, r1

  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_IRQ)  // IRQ mode
  mov   sp, fp
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_ABT)  // Abort mode
  mov   sp, fp
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_UND)  // Undef mode
  mov   sp, fp

  // Set the kernel supervisor mode stack pointer for this CPU
  // sp = kstack_top - (KSTACK_SIZE * <k_cpu_id>)
  msr   CPSR_c, #(PSR_I | PSR_F | PSR_M_SVC)
  ldr   sp, =kstack_top
  mov   r1, #KSTACK_SIZE
  mul   r1, r0
  sub   sp, r1

  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov   r11, #0

  // APs wait until the BSP finished its initialization
  cmp   r0, #0
  bne   ap_wait

  // BSP calls arch_init().
  mov   r0, r5
  ldr   r2, =arch_init
  blx   r2
  b     .

ap_wait:
  ldr   r1, =bsp_started
  ldrex r2, [r1]
  cmp   r2, #0
  beq   ap_wait

  // Call mp_enter()
  ldr   r1, =arch_mp_init
  blx   r1
  b     .

.bss

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
