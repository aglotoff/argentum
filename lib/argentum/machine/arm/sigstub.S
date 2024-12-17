#include <sys/syscall.h>

.globl __sigstub
__sigstub:
  add     lr, r0, #28       // skip handler, info, uc_link, uc_sigmask, r0

  // Save m_context
  stmia   lr!, {r1-r12}     // save R1-R12
  add     lr, #16           // SP, LR, PC, and PSR saved by the kernel
  vstmia  lr!, {s0-s31}     // save FPU registers
  vmrs    r1, fpscr         // FPSCR -> R0
  str     r1, [lr]          // save FPSCR
  
  mov     r4, r0
  mov     r5, lr

  // handler(signo, siginfo, ucontext)
  ldr     lr, [r4]          // handler -> LR
  ldr     r0, [r4, #4]      // signo -> R0
  add     r1, r4, #4        // siginfo -> R1
  add     r2, r4, #16       // ucontext -> R2
  blx     lr

  mov     lr, r5
  mov     r0, r4

  // Restore m_context
  ldr     r1, [lr]          // FPSCR -> R0
  vmsr    fpscr, r1         // restore FPSCR
  vldmdb  lr!, {s0-s31}     // restore FPU registers
  sub     lr, #16
  ldmdb   lr!, {r1-r12}     // restore R1-R12

  svc     #__SYS_SIGRETURN
