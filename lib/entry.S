/*
 * Entry point for user-mode processes
 */
  .text
  .globl  _start
_start:
  bl      main      // Call the user main routine.
  bl      exit      // Exit gracefully, passing the returned value as status.
  b       .         // Should not return.
