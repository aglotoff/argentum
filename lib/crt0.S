/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov     r11, #0

  // Save argc and argv.
  stmdb   sp!, {r0,r1}

  // Run the global constructors.
	bl      _init

  // Restore argc and argv.
  ldmia   sp!, {r0,r1}

  // Call the user main routine.
  bl      main

  // Terminate the process with the exit code from main().
  bl      exit

  // Should not return.
  b       .         
