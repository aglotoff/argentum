#include <string.h>

#include <kernel/process.h>
#include <kernel/page.h>

#include <arch/trap.h>

void
arch_task_init_stack(struct KTask *task, void (*entry)(void))
{
  uint8_t *sp = (uint8_t *) task->kstack + PAGE_SIZE;

  // Allocate space for user-mode trap frame
  if (task->ext != NULL) {
    struct Thread *thread = (struct Thread *) task->ext;

    sp -= sizeof(struct TrapFrame);
    thread->tf = (struct TrapFrame *) sp;
    memset(thread->tf, 0, sizeof(struct TrapFrame));
  }

  // Initialize the kernel-mode task context
  sp -= sizeof(struct Context);
  task->context = (struct Context *) sp;
  memset(task->context, 0, sizeof(struct Context));
  task->context->lr = (uint32_t) entry;
}

void
arch_task_idle(void)
{
  asm volatile("wfi");
}
