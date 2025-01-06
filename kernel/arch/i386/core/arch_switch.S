/*
 * ----------------------------------------------------------------------------
 * void k_arch_switch(struct Context **old, struct Context *new);
 * ----------------------------------------------------------------------------
 *
 * Save the current registers on the stack, effectively creating a
 * struct Context, and save its address to the memory location pointed to by
 * 'old'. Then switch to new stack and restore previously saved registers.
 *
 */
  .globl k_arch_switch
k_arch_switch:
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # Save old callee-save registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Save old floating-point registers
  movl  %esp, %ebp
  andl  $0xfffffff0, %ebp
  subl  $512, %ebp
  fxsave (%ebp)

  # Switch stacks
  movl %esp, (%eax)
  movl %edx, %esp

  # Load new floating-point registers
  movl  %esp, %ebp
  andl  $0xfffffff0, %ebp
  subl  $512, %ebp
  fxrstor (%ebp)

  # Load new callee-save registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp

  ret
