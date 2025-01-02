#include <sys/syscall.h>

.globl __sigstub
__sigstub:
  jmp .
