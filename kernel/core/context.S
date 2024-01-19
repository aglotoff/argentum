/*
 * ----------------------------------------------------------------------------
 * void arch_context_switch(struct Context **old, struct Context *new);
 * ----------------------------------------------------------------------------
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address to the memory location pointed to by
 * 'old'. Then switch to new stack and restore previously saved registers.
 *
 */
  .globl arch_context_switch
arch_context_switch:
  // Save old registers
  stmdb   sp!, {r4-r11,lr}  // save R4-R11 and LR
  vmrs    lr, fpscr         // FPSCR -> LR
  str     lr, [sp, #-4]!    // save FPSCR
  vstmdb  sp!, {s0-s31}     // save FPU registers

  // Switch stacks 
  str     sp, [r0]
  mov     sp, r1

  // Load new registers and switch to the new task
  vldmia  sp!, {s0-s31}     // restore FPU registers
  ldr     lr, [sp], #4      // FPSCR -> LR
  vmsr    fpscr, lr         // restore FPSCR
  ldmia   sp!, {r4-r11,lr}  // restore R4-R11 and LR

  // Return to the caller
  bx      lr
