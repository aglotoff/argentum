#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/vmspace.h>

int
sys_arch_get_num(void)
{
  struct KTask *current = k_task_current();
  return current->tf->eax;
}

int32_t
sys_arch_get_arg(int n)
{
  struct KTask *current = k_task_current();
  
  switch (n) {
  case 0:
    return current->tf->edx;
  case 1:
    return current->tf->ecx;
  case 2:
    return current->tf->ebx;
  case 3:
    return current->tf->edi;
  case 4:
    return current->tf->esi;
  case 5:
    panic("not implemented");
    // fall through
  default:
    return -1;
  }
}
