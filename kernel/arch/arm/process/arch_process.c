#include <kernel/process.h>
#include <kernel/core/assert.h>
#include <kernel/vmspace.h>

struct FpuContext {
  uint32_t s[32];
  uint32_t fpexc;
  uint32_t fpscr;
};

static inline void
fpu_context_save(void *dst)
{
  int tmp;

  asm volatile(
    "\tvstmia %[dst]!, {s0-s31}\n"
    "\tvmrs   %[tmp], fpexc\n"
    "\tstr    %[tmp], [%[dst]], #4\n"
    "\tvmrs   %[tmp], fpscr\n"
    "\tstr    %[tmp], [%[dst]], #4\n"
    : [tmp] "=r"(tmp), [dst] "+r" (dst) :
    : "memory", "cc"
  );
}

static inline void
fpu_context_restore(void *src)
{
  int tmp;

  asm volatile(
    "\tvldmia %[src]!, {s0-s31}\n"
    "\tldr    %[tmp], [%[src]], #4\n"
    "\tvmsr   fpexc, %[tmp]\n"
    "\tldr    %[tmp], [%[src]], #4\n"
    "\tvmsr   fpscr, %[tmp]\n"
    : [tmp] "=r"(tmp), [src] "+r" (src) 
    :
    : "memory"
  );
}

int
arch_process_copy(struct Process *parent, struct Process *child)
{
  k_assert(parent->thread != NULL);
  k_assert(child->thread != NULL);

  *child->thread->tf = *parent->thread->tf;
  child->thread->tf->r0 = 0;

  return 0;
}

void
arch_on_thread_before_switch(struct Thread *thread)
{
  fpu_context_save(thread->task.kstack);

  arch_vm_switch(thread->process);
  arch_vm_load(thread->process->vm->pgtab);
}

void
arch_on_thread_after_switch(struct Thread *thread)
{
  k_assert((uint8_t *) thread->task.context >= ((uint8_t *) thread->task.kstack + sizeof(struct FpuContext)));

  fpu_context_restore(thread->task.kstack);

  arch_vm_load_kernel();
}
