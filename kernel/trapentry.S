#include "armv7.h"
#include "memlayout.h"
#include "trap.h"

.section .text

/*
 *
 * Entry points for different traps
 *
 */

  .globl trap_undef
trap_undef:
  sub     lr, lr, #4              // lr_und -= 4
  str     lr, [sp, #8]            // save lr_und
  mov     lr, #T_UNDEF            // save trap number
  b       alltraps

  .globl trap_pabt
trap_pabt:
  sub     lr, lr, #4              // lr_abt -= 4
  str     lr, [sp, #8]            // save lr_abt
  mov     lr, #T_PABT             // save trap number
  b       alltraps

  .globl trap_dabt
trap_dabt:
  sub     lr, lr, #8              // lr_abt -= 8
  str     lr, [sp, #8]            // save lr_abt
  mov     lr, #T_DABT             // save trap number
  b       alltraps

  .globl trap_irq
trap_irq:
  sub     lr, lr, #4              // lr_irq -= 4
  str     lr, [sp, #8]            // save lr_irq
  mov     lr, #T_IRQ              // save trap number
  b       alltraps


/*
 *
 * All traps are sent here.
 *
 */

  .globl alltraps, trapret
alltraps:
  // Finish building the exception stack frame
  str     lr, [sp, #12]           // save trap number
  mrs     lr, spsr                // copy spsr
  str     lr, [sp, #4]            // save spsr
  str     r0, [sp, #0]            // save r0
  mov     r0, sp                  // copy sp

  // Switch to SVC mode
  mrs     lr, cpsr
  bic     lr, lr, #PSR_M_MASK
  orr     lr, lr, #PSR_M_SVC
  msr     cpsr_c, lr

  // Build the trap frame
  sub     sp, sp, #60             // leave space
  stmia   sp, {r1-r12}            // save r1-r12
  ldmia   r0, {r4-r7}             // retrieve data from the exception stack
  str     r4, [sp, #-4]!          // save r0
  stmdb   sp, {sp,lr}^            // save user mode sp and lr
  str     r7, [sp, #-12]!         // save trapno              
  str     r5, [sp, #64]           // save PSR
  str     lr, [sp, #68]           // save lr
  str     r6, [sp, #72]           // save pc

  // Make sure all stack frames are properly terminated
  mov     fp, #0

  // Call trap(tf), where tf=sp
  mov     r0, sp
  bl      trap

  // Return falls through to trapret:
trapret:
  add     sp, #12                 // skip trapno
  ldmdb   sp, {sp,lr}^            // restore user mode sp and lr
  ldmia   sp!, {r0-r12,lr}        // restore context
  msr     spsr_c, lr              // restore spsr
  ldmia   sp!, {lr,pc}^           // return


/*
 *
 * System call entry point.
 *
 */

  .globl trap_svc
trap_svc:
  stmdb   sp!, {r0,lr}
  mrs     lr, spsr
  stmdb   sp!, {r0-r12,lr}        // Save r0-r12
  sub     sp, #12                 // 
  mov     r0, #T_SWI              // save trap number
  stmia   sp, {r0,sp,lr}^         // save user mode sp and lr

  // Make sure all stack frames are properly terminated
  mov     fp, #0

  // Call trap(tf), where tf=sp
  mov     r0, sp
  bl      trap

  b       trapret
