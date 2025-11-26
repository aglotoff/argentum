#include <kernel/core/config.h>
#include <kernel/core/task.h>

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
  // Below is floating-point context aligned on 16-byte boundary
  k_uint32_t edi;
  k_uint32_t esi;
  k_uint32_t ebx;
  k_uint32_t ebp;
  k_uint32_t eip;
};

void
k_arch_task_init_stack(struct KTask *task, void (*entry)(void))
{
  k_uint8_t *sp = (k_uint8_t *) task->kstack + task->kstack_size;

  // Initialize the kernel-mode task context
  sp -= sizeof(struct Context);
  task->context = (struct Context *) sp;
  k_memset(task->context, 0, sizeof(struct Context));
  ((struct Context *) task->context)->eip = (k_uint32_t) entry;
}

void
k_arch_task_idle(void)
{
  // do nothing
}
