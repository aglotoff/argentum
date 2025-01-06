#include <sys/syscall.h>

.globl __sigstub
__sigstub:
  movl    %edi, 0x00(%eax)
  movl    %esi, 0x04(%eax)
  movl    %ebp, 0x08(%eax)
  movl    %esp, 0x0C(%eax)
  movl    %ebx, 0x10(%eax)
  movl    %edx, 0x14(%eax)
  movl    %ecx, 0x18(%eax)

  fxsave  0x34(%eax)
  pushl   %eax

  movl    0x4(%esp), %eax   # handler -> EAX
  movl    %esp, %ecx   
  addl    $0x8, %ecx        # info -> ECX
  movl    (%ecx), %ebx      # signo -> EBX

  movl    %ecx, %edx
  addl    $0xC, %edx        # ucontext -> EDX

  pushl   %edx
  pushl   %ecx
  pushl   %ebx

  call    *%eax

  addl    $12, %esp

  popl    %eax
  fxrstor 0x34(%eax)

  movl    0x18(%eax),%ecx
  movl    0x14(%eax),%edx
  movl    0x10(%eax),%ebx
  movl    0x0C(%eax),%esp
  movl    0x08(%eax),%ebp
  movl    0x04(%eax),%esi
  movl    0x00(%eax),%edi

  movl    $__SYS_SIGRETURN, %eax
  int     $0x80
