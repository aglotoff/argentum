#include <string.h>

#include <kernel/task.h>
#include <kernel/page.h>
#include <kernel/console.h>

#include <arch/trap.h>

void
arch_task_init_stack(struct KTask *task, void (*entry)(void))
{
  uint8_t *sp = (uint8_t *) task->kstack + PAGE_SIZE;

  // Allocate space for user-mode trap frame
  if (task->thread != NULL) {
    sp -= sizeof(struct TrapFrame);
    task->tf = (struct TrapFrame *) sp;
    memset(task->tf, 0, sizeof(struct TrapFrame));
  }

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
