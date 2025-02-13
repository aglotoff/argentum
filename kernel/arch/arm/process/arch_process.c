#include <kernel/process.h>
#include <kernel/assert.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  assert(parent->thread != NULL);
  assert(child->thread != NULL);

  *child->thread->task->tf = *parent->thread->task->tf;
  child->thread->task->tf->r0 = 0;

  return 0;
}
