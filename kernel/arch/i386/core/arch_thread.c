#include <string.h>

#include <kernel/core/task.h>

void
arch_task_init_stack(struct KTask *task, void (*entry)(void))
{
  uint8_t *sp = (uint8_t *) task->kstack + task->kstack_size;

  // Initialize the kernel-mode task context
  sp -= sizeof(struct Context);
  task->context = (struct Context *) sp;
  memset(task->context, 0, sizeof(struct Context));
  task->context->eip = (uint32_t) entry;
}

void
arch_task_idle(void)
{
  // do nothing
}
