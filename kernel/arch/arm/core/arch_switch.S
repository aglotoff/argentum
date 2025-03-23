/*
 * ----------------------------------------------------------------------------
 * void k_arch_switch(struct Context **old, struct Context *new);
 * ----------------------------------------------------------------------------
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address to the memory location pointed to by
 * 'old'. Then switch to new stack and restore previously saved registers.
 *
 */
  .globl k_arch_switch
k_arch_switch:
  // Save old registers
  stmdb   sp!, {r4-r11,lr}  // save R4-R11 and LR

  // Switch stacks 
  str     sp, [r0]
  mov     sp, r1

  // Load new registers and switch to the new task
  ldmia   sp!, {r4-r11,lr}  // restore R4-R11 and LR

  // Return to the caller
  bx      lr
