#include <kernel/process.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  *child->task->tf = *parent->task->tf;
  child->task->tf->r0 = 0;

  return 0;
}
