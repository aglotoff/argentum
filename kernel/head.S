#include "cp15.h"
#include "memlayout.h"

#define RELOC(x)  ((x) - KERNEL_BASE)

.section .boot, "a"

# Kernel entry point
  .globl  _start
_start:
  # Load the physical address of the initial translation table
  ldr r0, =RELOC(boot_trtab)
  mcr p15, 0, r0, c2, c0, 0

  # Assign domain access
  ldr r0, =DACR_D(0, DA_CLIENT)
  mcr p15, 0, r0, c3, c0, 0

  # Enable cache and MMU
  mrc p15, 0, r0, c1, c0, 0
  ldr r1, =(SCTLR_M | SCTLR_C | SCTLR_I)
  orr r0, r0, r1
  mcr p15, 0, r0, c1, c0, 0

  # Set the stack pointer
  ldr sp, =kstack_top

  # Clear the frame pointer register (R11) so that stack backtraces will be
  # terminated properly
  mov r11, #0

  # Jump to C code
  ldr r0, =main
  blx r0

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
