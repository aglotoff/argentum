/*
 * void context_switch(struct context **old, struct context *new);
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address in old. Then switch to new stack and
 * restore previously saved registers.
 */
  .globl context_switch
context_switch:
  stmdb sp!, {r4-r11,lr}    // Save old callee-saved registers
  str   sp, [r0]            // Save old stack poninter
  mov   sp, r1              // Load new stack pointer
  ldmia sp!, {r4-r11,lr}    // Load new calee-saved registers
  bx    lr                  // Return
