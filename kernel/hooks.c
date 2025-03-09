#include <kernel/core/task.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>

void
on_task_destroy(struct KTask *task)
{
  if (task->ext != NULL)
    thread_on_destroy((struct Thread *) task->ext);
}

void
on_task_before_switch(struct KTask *task)
{
  if (task->ext != NULL) {
    struct Thread *thread = (struct Thread *) task->ext;

    k_assert(&thread->task == task);

    if (thread->process != NULL) {
      asm volatile("fxrstor (%0)" : : "r" (task->kstack));

      arch_vm_switch(thread->process);
      arch_vm_load(thread->process->vm->pgtab);
    }
  }
}

void
on_task_after_switch(struct KTask *task)
{
  if (task->ext != NULL) {
    k_assert((uint8_t *) task->context >= ((uint8_t *) task->kstack + 512));
    asm volatile("fxsave (%0)" : : "r" (task->kstack));

    arch_vm_load_kernel();
  }
}

void
on_task_idle(void)
{
  thread_idle();
}
