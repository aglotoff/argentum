/*
 * void context_switch(struct context **old, struct context *new);
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address in old. Then switch to new stack and
 * restore previously saved registers.
 */
  .globl context_switch
context_switch:
  // Save old callee-saved registers
  stmdb   sp!, {r4-r11,lr}

  // Save old FPU registers
  vmrs    r4, fpscr
  stmdb   sp!, {r4}
  vstmdb  sp!, {s0-s31}

  // Save old stack pointer
  str     sp, [r0]

  // Load new stack pointer
  mov     sp, r1              

  // Load new FPU registers
  vldmia  sp!, {s0-s31}
  ldmia   sp!, {r4}
  vmsr    fpscr, r4

  // Load new calee-saved registers
  ldmia   sp!, {r4-r11,lr}

  // Switch to the new task
  bx      lr                  
