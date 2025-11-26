#include <kernel/core/config.h>
#include <kernel/core/task.h>

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
  k_uint32_t r4;
  k_uint32_t r5;
  k_uint32_t r6;
  k_uint32_t r7;
  k_uint32_t r8;
  k_uint32_t r9;
  k_uint32_t r10;
  k_uint32_t r11;
  k_uint32_t lr;
};

void
k_arch_task_init_stack(struct KTask *task, void (*entry)(void))
{
  k_uint8_t *sp = (k_uint8_t *) task->kstack + task->kstack_size;

  // Initialize the kernel-mode task context
  sp -= sizeof(struct Context);
  task->context = (struct Context *) sp;
  memset(task->context, 0, sizeof(struct Context));
  ((struct Context *) task->context)->lr = (k_uint32_t) entry;
}

void
k_arch_task_idle(void)
{
  asm volatile("wfi");
}
