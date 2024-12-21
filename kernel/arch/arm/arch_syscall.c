#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/vmspace.h>

// Extract the system call number from the SVC instruction opcode
int
sys_arch_get_num(void)
{
  struct Process *current = process_current();

  int *pc = (int *) (current->thread->tf->pc - 4);
  int r;

  if ((r = vm_user_check_buf(current->vm->pgtab, (uintptr_t) pc, sizeof(int), VM_READ)) < 0)
    return r;

  return *pc & 0xFFFFFF;
}

// Get the n-th argument from the current process' trap frame.
// Support up to 6 system call arguments
int32_t
sys_arch_get_arg(int n)
{
  struct Process *current = process_current();

  switch (n) {
  case 0:
    return current->thread->tf->r0;
  case 1:
    return current->thread->tf->r1;
  case 2:
    return current->thread->tf->r2;
  case 3:
    return current->thread->tf->r3;
  case 4:
    return current->thread->tf->r4;
  case 5:
    return current->thread->tf->r5;
  default:
    panic("Invalid argument number: %d", n);
    return 0;
  }
}
