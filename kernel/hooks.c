#include <kernel/core/task.h>

#include <kernel/console.h>
#include <kernel/kdebug.h>
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
on_task_switch(struct KTask *prev, struct KTask *next)
{
  struct Thread *prev_thread = prev == NULL ? NULL : (struct Thread *) prev->ext;
  struct Thread *next_thread = next == NULL ? NULL : (struct Thread *) next->ext;

  k_assert((prev_thread == NULL) || (&prev_thread->task == prev));
  k_assert((next_thread == NULL) || (&next_thread->task == next));

  if (prev_thread != NULL)
    arch_on_thread_after_switch(prev_thread);

  if ((next_thread == NULL) || (next_thread->process == NULL)) {
    arch_vm_load_kernel();
  } else {
    arch_on_thread_before_switch(next_thread);
    arch_vm_load(next_thread->process->vm->pgtab);
  }
}

void
on_sched_idle(void)
{
  thread_idle();
}

void
on_spinlock_trace_print(struct KSpinLock *spin)
{
  int i;

  for (i = 0; i < K_SPINLOCK_MAX_PCS && spin->pcs[i]; i++) {
    struct PcDebugInfo info;

    debug_info_pc(spin->pcs[i], &info);
    cprintf("  [%p] %s (%s at line %d)\n",
            spin->pcs[i],
            info.fn_name, info.file, info.line);
  }
}
