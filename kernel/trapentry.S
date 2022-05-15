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
  sub     lr, #4            // PC = LR_und - 4
  str     lr, [sp, #12]     // Save PC
  mov     lr, #T_UNDEF      // trap number -> LR
  b       trap_common

// Prefetch Abort
  .globl trap_pabt
trap_pabt:
  sub     lr, #4            // PC = LR_abt - 4
  str     lr, [sp, #12]     // Save PC
  mov     lr, #T_PABT       // trap number -> LR
  b       trap_common

// Data Abort
  .globl trap_dabt
trap_dabt:
  sub     lr, #8            // PC = LR_abt - 8
  str     lr, [sp, #12]     // Save PC
  mov     lr, #T_DABT       // trap number -> LR
  b       trap_common

// IRQ (Interrupt)
  .globl trap_irq
trap_irq:
  sub     lr, #4            // PC = LR_irq - 4
  str     lr, [sp, #12]     // Save PC
  mov     lr, #T_IRQ        // trap number -> LR
  b       trap_common

// System call
  .globl trap_svc
trap_svc:
  // Start building the user-mode trap frame (struct TrapFrame)
  stmdb   sp!, {r0-r12,lr}  // R0-R12 and PC
  stmdb   sp, {sp,lr}^      // save SP_usr and LR_usr
  sub     sp, #8
  mov     r1, #T_SWI        // trapno -> R1
  mrs     r0, spsr          // SPSR -> R0
  stmdb   sp!, {r0-r1}      // save PSR and trapno

  b       trap_user_common

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
  str     lr, [sp, #4]      // Save trapno
  mrs     lr, spsr          // SPSR -> LR
  str     lr, [sp, #0]      // Save PSR
  str     r0, [sp, #8]      // Save R0
  mov     r0, sp            // save pointer to the resulting sructure in R0

  // If the exception happened in user mode, branch to trap_user
  and     lr, #PSR_M_MASK
  cmp     lr, #PSR_M_USR
  beq     trap_user

/*
 * ----------------------------------------------------------------------------
 * Kernel-mode trap handler
 * ----------------------------------------------------------------------------
 */

trap_kernel:
  // Switch to SVC mode
  mrs     lr, cpsr
  bic     lr, lr, #PSR_M_MASK
  orr     lr, lr, #(PSR_M_SVC | PSR_I | PSR_F)
  msr     cpsr, lr

  // Build the kernel-mode trap frame (struct TrapFrame)
  str     lr, [sp, #-60]    // save LR_svc
  str     sp, [sp, #-64]    // save SP_svc
  ldr     lr, [r0, #12]     // PC from exception stack -> LR
  stmdb   sp!, {r1-r12,lr}  // save R1-R12 and PC
  ldmia   r0, {r1-r3}       // PSR, trapno, R0 from exception stack -> R1-R3
  str     r3, [sp, #-4]!    // save R0
  str     r2, [sp, #-12]!   // save trapno
  str     r1, [sp, #-4]!    // save PSR

  // Call trap(tf)
  mov     r0, sp
  bl      trap

  // Restore the kernel state using the contents of the trap frame
  ldr     r0, [sp], #12     // PSR -> R0, skip trapno and SP
  msr     spsr, r0          // restore SPSR
  ldr     lr, [sp], #4      // restore LR
  ldmia   sp!, {r0-r12,pc}^ // restore R0-R12, PC and return from the trap

/*
 * ----------------------------------------------------------------------------
 * User-mode trap handler
 * ----------------------------------------------------------------------------
 */

trap_user:
  // Switch to SVC mode
  mrs     lr, cpsr
  bic     lr, lr, #PSR_M_MASK
  orr     lr, lr, #(PSR_M_SVC | PSR_I | PSR_F)
  msr     cpsr, lr

  // Start building the user-mode trap frame (struct TrapFrame)
  ldr     lr, [r0, #12]     // saved PC -> LR
  stmdb   sp!, {r1-r12,lr}  // save R1-R12 and PC
  ldmia   r0, {r1-r3}       // PSR -> R1, trapno -> R2, R0 -> R3
  str     r3, [sp, #-4]!    // save R0
  stmdb   sp, {sp,lr}^      // save user SP and LR
  str     r2, [sp, #-12]!   // save trapno
  str     r1, [sp, #-4]!    // save PSR

  // fall through

trap_user_common:
  // Save pointer to the previously built structure
  mov     r0, sp

  // Make sure all stack frames are properly terminated
  mov     fp, #0

  // Call trap(tf)
  bl      trap

  // Restore the user state using the contents of the trap frame
  ldr     lr, [sp], #16     // PSR -> LR; skip trapno, SP_usr, and LR_usr
  msr     spsr, lr          // restore SPSR
  ldmdb   sp, {sp,lr}^      // restore SP_usr and LR_usr
  ldmia   sp!, {r0-r12,pc}^ // restore R0-R12, PC and return from the trap
