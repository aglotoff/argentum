/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov     r11, #0

  // Save program name into the global variable
  ldr     r3, =__progname
  cmp     r1, #0
  beq     1f
  ldr     r4, [r1]
  cmp     r4, #0
  strne   r4, [r3]
1:

  // Save environment into the global variable
  ldr     r3, =environ
  cmp     r2, #0
  strne   r2, [r3]

  stmdb   sp!,{r0-r2}

  bl      __libc_init_array

  ldmia   sp!,{r0-r2}

  // Call the user main routine.
  bl      main

  // Terminate the process with the exit code from main().
  bl      exit

  // Should not return.
  b       .         
