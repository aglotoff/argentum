#include <argentum/syscall.h>

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

#include <arch/kernel/trap.h>

#define SWI_OPCODE      0xEF000000
#define SWI_OPCODE_MASK 0xFF000000

int
arch_syscall_no(void *ctx)
{
  struct TrapFrame *tf = (struct TrapFrame *) ctx;
  uintptr_t swi;
  int err;

  err = vm_copy_in(thread_current()->process->vm, tf->pc - 4, &swi, sizeof swi);
  if (err < 0)
    return err;

  if ((swi & SWI_OPCODE_MASK) != SWI_OPCODE)
    return -EINVAL;

  return swi & ~SWI_OPCODE_MASK;
}

long
arch_syscall_arg(void *ctx, int n)
{
  struct TrapFrame *tf = (struct TrapFrame *) ctx;

  switch (n) {
  case 0:
    return tf->r0;
  case 1:
    return tf->r1;
  case 2:
    return tf->r2;
  case 3:
    return tf->r3;
  default:
    if (n < 0)
      panic("Invalid argument number: %d", n);

    // TODO: grab additional parameters from the user stack.
    return 0;
  }
}
