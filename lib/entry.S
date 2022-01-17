/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  // Clear the frame pointer register (R11) so that stack backtraces will be
  // terminated properly.
  mov   r11, #0

  bl      main      // Call the user main routine.
  bl      exit      // Exit gracefully, passing the returned value as status.
  b       .         // Should not return.
