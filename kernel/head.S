#include "memlayout.h"

.text

# Kernel entry point
  .globl  _start
_start:
  # Set th stack pointer
  ldr sp, =kstack_top

  # Jump to C code
  bl main

  # Should never return here
1:
  b 1b

.bss

# Kernel stack
  .globl    kstack, kstack_top
  .p2align  12
kstack:
  .space  KSTACK_SIZE
kstack_top:
