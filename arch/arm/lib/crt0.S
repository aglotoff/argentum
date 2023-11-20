/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov     r11, #0

  // Call the user main routine.
  bl      main

  // Should not return.
1:
  bl      _exit
  b       1b

