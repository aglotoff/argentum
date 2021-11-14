/*
 *
 * int setjmp(jmp_buf env);
 *
 */
.globl setjmp
setjmp:
  // Save the required registers in env
  stmia r0, {r4-r11,ip,sp,lr}

  // Return 0
  mov   r0, #0                  
  bx    lr
