#include <kernel/process.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  *child->task->tf = *parent->task->tf;
  child->task->tf->eax = 0;
  return 0;
}
