/*
 * ----------------------------------------------------------------------------
 * void context_switch(struct Context **old, struct Context *new);
 * ----------------------------------------------------------------------------
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address to the memory location pointed to by
 * 'old'. Then switch to new stack and restore previously saved registers.
 *
 */
  .globl context_switch
context_switch:
  // Save old registers
  stmdb   sp!, {r4-r11,lr}

  // Switch stacks 
  str     sp, [r0]
  mov     sp, r1

  // Load new registers and switch to the new task
  ldmia   sp!, {r4-r11,lr}    
  bx      lr     
