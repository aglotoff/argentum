#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/vmspace.h>

int
sys_arch_get_num(void)
{
  struct Process *current = process_current();
  return current->thread->tf->eax;
}

int32_t
sys_arch_get_arg(int n)
{
  struct Process *current = process_current();
  
  switch (n) {
  case 0:
    return current->thread->tf->edx;
  case 1:
    return current->thread->tf->ecx;
  case 2:
    return current->thread->tf->ebx;
  case 3:
    return current->thread->tf->edi;
  case 4:
    return current->thread->tf->esi;
  case 5:
    panic("not implemented");
    // fall through
  default:
    return -1;
  }
}
