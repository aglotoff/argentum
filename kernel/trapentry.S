#include <kernel/armv7.h>
#include <kernel/mm/memlayout.h>
#include <kernel/trap.h>

.section .text

/*
 * ----------------------------------------------------------------------------
 * Trap entry points
 * ----------------------------------------------------------------------------
 */

// Undefined Instruction
  .globl trap_undef
trap_undef:
  sub     lr, #4                // LR_und -= 4
  str     lr, [sp, #12]         // save LR_und
  mov     lr, #T_UNDEF          // save trap number
  b       trap_common

// Prefetch Abort
  .globl trap_pabt
trap_pabt:
  sub     lr, #4                // LR_abt -= 4
  str     lr, [sp, #12]         // save LR_abt
  mov     lr, #T_PABT           // save trap number
  b       trap_common

// Data Abort
  .globl trap_dabt
trap_dabt:
  sub     lr, #8                // LR_abt -= 8
  str     lr, [sp, #12]         // save LR_abt
  mov     lr, #T_DABT           // save trap number
  b       trap_common

// IRQ (Interrupt)
  .globl trap_irq
trap_irq:
  sub     lr, #4                // LR_irq -= 4
  str     lr, [sp, #12]         // save LR_irq
  mov     lr, #T_IRQ            // save trap number
  b       trap_common

/*
 * ----------------------------------------------------------------------------
 * Common trap handler code
 * ----------------------------------------------------------------------------
 *
 * All traps (except system calls) jump here.
 *
 */

  .globl trap_common, trap_exit
trap_common:
  // Finish building the exception stack frame
  str     lr, [sp, #0]          // save the trap number
  mrs     lr, spsr              // copy SPSR
  str     lr, [sp, #4]          // save PSR
  str     r0, [sp, #8]          // save R0
  mov     r0, sp                // save the resulting pointer in R0

  // Switch to SVC mode
  mrs     lr, cpsr
  bic     lr, lr, #PSR_M_MASK
  orr     lr, lr, #PSR_M_SVC
  msr     cpsr_c, lr

  // Build the trap frame (see struct TrapFrame)
  ldr     lr, [r0, #12]         // load PC
  stmdb   sp!, {r1-r12,lr}      // save R1-R12 and PC
  ldmia   r0, {r0-r2}           // retrieve data from the exception stack
  stmdb   sp!, {r0-r2}          // save trapno, PSR, and R0

  // Save pointer to the resulting structure in R0
  mov     r0, sp

  // If trap occured in user mode, jump to trap_user
  bic     r1, #PSR_M_MASK
  cmp     r1, #PSR_M_USR
  beq     trap_user

  bl      trap

  // Fall through to trap_exit

trap_exit:
  add     sp, #4                // skip trapno
  ldmia   sp!, {lr}             // load SPSR
  msr     spsr, lr              // restore PSR
  ldmia   sp!, {r0-r12,pc}^     // restore registers and return

/*
 * ----------------------------------------------------------------------------
 * System call entry point
 * ----------------------------------------------------------------------------
 */

  .globl trap_svc
trap_svc:
  // Build the trap frame (see struct UTrapFrame)
  stmdb   sp!, {r0-r12,lr}      // save R0-R12 and PC
  mrs     lr, spsr              // copy SPSR
  stmdb   sp!, {lr}             // save SPSR
  mov     r0, #T_SWI            // save trap number
  stmdb   sp!, {r0}

  // Save pointer to the resulting structure in R0
  mov     r0, sp

  // Fall through to trap_user

trap_user:
  // Save user-mode SP and LR
  sub     sp, #8
  stmia   sp, {sp,lr}^            

  // Save FPU registers
  vmrs    r1, fpscr
  stmdb   sp!, {r1}
  vstmdb  sp!, {s0-s31}

  // Make sure all stack frames are properly terminated
  mov     fp, #0

  // Call trap(tf), where tf=sp
  bl      trap

  // Load FPU registers
  vldmia  sp!, {s0-s31}
  ldmia   sp!, {r2}
  vmsr    fpscr, r2

  // Restore user-mode SP and LR
  ldmia   sp, {sp,lr}^
  add     sp, #8

  b       trap_exit
