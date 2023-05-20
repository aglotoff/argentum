/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov     r11, #0

  // Save argc, argv, and environ
  stmdb   sp!, {r0,r1,r2}

  // Run the global constructors.
	bl      _init

  // Restore argc and argv.
  ldmia   sp!, {r0,r1,r2}

  // Save environment into the global variable
  ldr     r3, =environ
  cmp     r2, #0
  strne   r2, [r3]

  // Save program name into the global variable
  ldr     r3, =__progname
  cmp     r1, #0
  beq     1f
  ldr     r4, [r1]
  cmp     r4, #0
  strne   r4, [r3]

1:
  // Call the user main routine.
  bl      main

  // Terminate the process with the exit code from main().
  bl      exit

  // Should not return.
  b       .         

  .data
  .globl __progname
__progname:
  .long   0
