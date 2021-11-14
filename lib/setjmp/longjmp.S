/*
 *
 * void longjmp(jmp_buf env, int val);
 *
 */
.globl longjmp
longjmp:
  // Restore the registers saved in env
  ldmia r0, {r4-r11,ip,sp,lr}  

  // If val is 0, return 1; otherwise return val
  cmp   r1, #0
  addeq r1, #1
  mov   r0, r1
  bx    lr
