#include <kernel/process.h>
#include <kernel/core/assert.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  k_assert(parent->thread != NULL);
  k_assert(child->thread != NULL);

  *child->thread->tf = *parent->thread->tf;
  child->thread->tf->eax = 0;
  return 0;
}
