#include <sys/syscall.h>

.globl __sigstub
__sigstub:
  // Save context
  stmdb   sp!, {r1-r12}     // save R1-R12
  vmrs    lr, fpscr         // FPSCR -> LR
  str     lr, [sp, #-4]!    // save FPSCR
  vstmdb  sp!, {s0-s31}     // save FPU registers

  # Call handler
  mov     r12, r0
  ldr     r0, [r12]
  ldr     lr, [r12, #4]
  blx     lr

  // Restore context
  vldmia  sp!, {s0-s31}     // restore FPU registers
  ldr     lr, [sp], #4      // FPSCR -> LR
  vmsr    fpscr, lr         // restore FPSCR
  ldmia   sp!, {r1-r12}     // restore R1-R12

  svc     #__SYS_SIGRETURN
