/*
 * ----------------------------------------------------------------------------
 * void longjmp(jmp_buf env, int val);
 * ----------------------------------------------------------------------------
 */

.globl longjmp
longjmp:
  // Restore the registers saved in `env`
  vldmia  r0!, {s0-s31}           // restore FPU registers
  ldr     r2, [r0], #4            // FPSCR -> R1
  vmsr    fpscr, r2               // restore FPSCR
  ldmia   r0!, {r4-r11,ip,sp,lr}  // restore R4-R11, IP, SP, and LR

  // If `val` is 0, return 1; otherwise return `val`
  cmp     r1, #0
  addeq   r1, #1
  mov     r0, r1
  bx      lr
