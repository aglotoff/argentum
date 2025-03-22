#include <kernel/core/task.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>

void
on_task_destroy(struct KTask *task)
{
  if (task->ext != NULL)
    thread_on_destroy((struct Thread *) task->ext);
}

void arch_on_thread_before_switch(struct Thread *);
void arch_on_thread_after_switch(struct Thread *);

void
on_task_before_switch(struct KTask *task)
{
  if (task->ext != NULL) {
    struct Thread *thread = (struct Thread *) task->ext;

    k_assert(&thread->task == task);

    if (thread->process != NULL) {
      arch_on_thread_before_switch(thread);
    }
  }
}

void
on_task_after_switch(struct KTask *task)
{
  if (task->ext != NULL) {
    arch_on_thread_after_switch((struct Thread *) task->ext);
  }
}

void
on_task_idle(void)
{
  thread_idle();
}
