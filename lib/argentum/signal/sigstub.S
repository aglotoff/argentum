#include <sys/syscall.h>

.globl __sigstub
__sigstub:
  add     lr, sp, #12       // skip uc_link, uc_sigmask, R0

  // Save m_context
  stmia   lr!, {r1-r12}     // save R1-R12

  add     lr, #16           // SP, LR, PC, and PSR saved by the kernel

  vstmia  lr!, {s0-s31}     // save FPU registers
  vmrs    r0, fpscr         // FPSCR -> R0
  str     r0, [lr], #4      // save FPSCR
  
  mov     r5, lr
  mov     r4, lr

  ldr     lr, [r4], #4      // handler -> LR
  ldr     r0, [r4]          // signo -> R0
  mov     r1, r4            // siginfo -> R1
  mov     r2, sp            // ucontext -> R2

  // handler(signo, siginfo, ucontext)
  blx     lr

  mov     lr, r5

  // Restore m_context
  ldr     r0, [lr, #-4]!    // FPSCR -> R0
  vmsr    fpscr, r0         // restore FPSCR
  vldmdb  lr!, {s0-s31}     // restore FPU registers
  sub     lr, #16
  ldmdb   lr!, {r1-r12}     // restore R1-R12

  svc     #__SYS_SIGRETURN
