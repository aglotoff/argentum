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
  stmdb   sp!, {r0-r3}      // save R0-R3
  sub     r3, lr, #4        // R3 = LR_und - 4
  mov     r1, #T_UNDEF      // trap number -> R1
  b       trap_common

// Prefetch Abort
  .globl trap_pabt
trap_pabt:
  stmdb   sp!, {r0-r3}      // save R0-R3
  sub     r3, lr, #4        // R3 = LR_und - 4
  mov     r1, #T_PABT       // trap number -> R1
  b       trap_common

// Data Abort
  .globl trap_dabt
trap_dabt:
  stmdb   sp!, {r0-r3}      // save R0-R3
  sub     r3, lr, #8        // R3 = LR_und - 4
  mov     r1, #T_DABT       // trap number -> R1
  b       trap_common

// IRQ (Interrupt)
  .globl trap_irq
trap_irq:
  stmdb   sp!, {r0-r3}      // save R0-R3
  sub     r3, lr, #4        // R3 = LR_und - 4
  mov     r1, #T_IRQ        // trap number -> R1
  b       trap_common

// System call
  .globl trap_svc
trap_svc:
  // Start building the user-mode trap frame (struct UTrapFrame)
  stmdb   sp!, {lr}         // save PC
  stmdb   sp, {sp,lr}^      // save user SP and LR
  sub     sp, #8
  stmdb   sp!, {r0-r12}     // save R0-R12 
  mov     r0, #T_SWI        // trap number -> R0
  mrs     r1, spsr          // SPSR -> R1
  stmdb   sp!, {r0,r1}      // save trapno and SPSR

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
  mrs     r2, spsr          // SPSR -> R2
  stmdb   sp!, {r1-r2}      // save trapno and SPSR
  mov     r0, sp            // save pointer to the resulting sructure in R0
  add     sp, #KXSTACK_SIZE // restore the exception ode stack pointer

  // Switch to SVC mode
  mrs     lr, cpsr
  bic     lr, lr, #PSR_M_MASK
  orr     lr, lr, #(PSR_M_SVC | PSR_I | PSR_F)
  msr     cpsr, lr

  // If the exception happened in user mode, branch to trap_user
  and     r2, #PSR_M_MASK
  cmp     r2, #PSR_M_USR
  beq     trap_user

/*
 * ----------------------------------------------------------------------------
 * Kernel-mode trap handler
 * ----------------------------------------------------------------------------
 */

trap_kernel:
  // Build the kernel-mode trap frame (struct TrapFrame)
  mov     r1, sp            // kernel SP -> R1
  mov     r2, lr            // kernel LR -> R2
  stmdb   sp!, {r1-r3}      // save kernel SP, LR, and PC
  stmdb   sp!, {r4-r12}     // save R4-R12 
  ldmia   r0, {r1-r6}       // get trapno, PSR, R0-R3 from the exception stack
  stmdb   sp!, {r1-r6}      // save trapno, PSR, R0-R3

  // Call trap(tf)
  mov     r0, sp
  bl      trap

  ldmia   sp!, {r0-r1}      // trapno -> R0 (ignored), SPSR -> R1
  msr     spsr, r1          // restore SPSR
  ldmia   sp!, {r0-r12,lr}  // restore R0-R12, kernel SP -> LR (ignored)
  ldmia   sp!, {lr,pc}^     // return from the trap handler

/*
 * ----------------------------------------------------------------------------
 * User-mode trap handler
 * ----------------------------------------------------------------------------
 */

trap_user:
  // Start building the user-mode trap frame (struct UTrapFrame)
  str     r3, [sp, #-4]!    // save user PC
  stmdb   sp, {sp,lr}^      // save user SP and LR
  sub     sp, #8
  stmdb   sp!, {r4-r12}     // save R4-R12 
  ldmia   r0, {r1-r6}       // get trapno, PSR, R0-R3 from the exception stack
  stmdb   sp!, {r1-r6}      // save trapno, PSR, R0-R3

  // fall through

trap_user_common:
  // Make sure all stack frames are properly terminated
  mov     fp, #0

  // Save pointer to the resulting structure
  mov     r0, sp

  // Finish building the trap frame
  vmrs    r1, fpscr         // FPSCR -> R1
  str     r1, [sp, #-4]!    // save FPSCR
  vstmdb  sp!, {s0-s31}     // save FPU registers

  // Call trap(tf)
  bl      trap

  vldmia  sp!, {s0-s31}     // restore FPU registers
  ldmia   sp!, {r0-r2}      // FPSCR -> R0, trapno -> R1 (ignore), SPSR -> R2
  vmsr    fpscr, r0         // restore FPSCR
  msr     spsr, r2          // restore SPSR
  ldmia   sp!, {r0-r12}     // restore R0-R12
  ldmia   sp, {sp,lr}^      // restore user SP and LR
  add     sp, #8
  ldmia   sp!, {pc}^        // return from the trap handler
