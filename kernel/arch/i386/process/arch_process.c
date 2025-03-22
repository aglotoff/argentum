#include <kernel/process.h>
#include <kernel/core/assert.h>
#include <kernel/vmspace.h>

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  k_assert(parent->thread != NULL);
  k_assert(child->thread != NULL);

  *child->thread->tf = *parent->thread->tf;
  child->thread->tf->eax = 0;
  return 0;
}

void
arch_on_thread_before_switch(struct Thread *thread)
{
  asm volatile("fxrstor (%0)" : : "r" (thread->task.kstack));

  arch_vm_switch(thread->process);
  arch_vm_load(thread->process->vm->pgtab);
}

void
arch_on_thread_after_switch(struct Thread *thread)
{
  k_assert((uint8_t *) thread->task.context >= ((uint8_t *) thread->task.kstack + 512));
  asm volatile("fxsave (%0)" : : "r" (thread->task.kstack));

  arch_vm_load_kernel();
}
