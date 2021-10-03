  .text

  .globl _start
_start:
  bl  main
  
1:
  bl  exit
  b   1b
