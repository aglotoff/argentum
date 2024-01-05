/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
#ifndef __thumb__
  mov     r11, #0
#endif

  // Call the user main routine.
  bl      main

  // Terminate the process with the exit code from main().
  bl      exit

  // Should not return.
  b       .         
