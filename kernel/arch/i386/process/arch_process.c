#include <kernel/process.h>
#include <kernel/core/assert.h>
#include <kernel/vmspace.h>
#include <arch/i386/lapic.h>

int
arch_process_copy(struct Thread *parent, struct Thread *child)
{
  *child->tf = *parent->tf;
  child->tf->eax = 0;

  return 0;
}

void arch_vm_switch(struct Thread *);  // in arch_vm.c

void
arch_on_thread_before_switch(struct Thread *thread)
{
  asm volatile("fxrstor (%0)" : : "r" (thread->task.kstack));
  arch_vm_switch(thread);
}

void
arch_on_thread_after_switch(struct Thread *thread)
{
  k_assert((uint8_t *) thread->task.context >= ((uint8_t *) thread->task.kstack + 512));
  asm volatile("fxsave (%0)" : : "r" (thread->task.kstack));
}

unsigned
k_cpu_id(void)
{
  return lapic_id();
}

static inline uint32_t
ebp_get(void)
{
	uint32_t ebp;

	asm volatile("movl %%ebp,%0" : "=r" (ebp));
	return ebp;
}

void
on_spinlock_trace_record(struct KSpinLock *spin)
{
  k_uintptr_t *ebp;
  int i;

  ebp = (k_uintptr_t *) ebp_get();

  for (i = 0; (ebp != NULL) && (i < K_SPINLOCK_MAX_PCS); i++) {
    spin->pcs[i] = ebp[1];
    ebp = (k_uintptr_t *) ebp[0];
  }

  for ( ; i < K_SPINLOCK_MAX_PCS; i++)
    spin->pcs[i] = 0;
}
