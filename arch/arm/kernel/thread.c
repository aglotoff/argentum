#include <string.h>

#include <kernel/page.h>
#include <kernel/thread.h>

#include <arch/kernel/regs.h>
#include <arch/kernel/trap.h>

void
arch_thread_idle(void)
{
  asm volatile("wfi");
}

void
arch_thread_create_user(struct Thread *thread,
                        void *entry,
                        void *arg1,
                        void *arg2,
                        void *arg3)
{
  uint8_t *sp = (uint8_t *) thread->kstack + thread->kstack_size;
  struct TrapFrame *tf;
  struct ThreadContext *context;
  
  sp -= sizeof(struct TrapFrame);
  tf = (struct TrapFrame *) sp;
  memset(tf, 0, sizeof(struct TrapFrame));
  tf->pc  = (uint32_t) entry;
  tf->r0  = (uint32_t) arg1;
  tf->r1  = (uint32_t) arg2;
  tf->r2  = (uint32_t) arg3;
  tf->sp  = VIRT_USTACK_TOP;
  tf->psr = PSR_M_USR | PSR_F;

  sp -= sizeof(struct ThreadContext);
  context = (struct ThreadContext *) sp;
  memset(context, 0, sizeof(struct ThreadContext));
  context->lr = (uint32_t) thread_run;

  thread->tf = tf;
  thread->context = context;
}

void
arch_thread_create(struct Thread *thread)
{
  uint8_t *sp = (uint8_t *) thread->kstack + thread->kstack_size;
  struct ThreadContext *context;

  sp -= sizeof(struct ThreadContext);
  context = (struct ThreadContext *) sp;
  memset(context, 0, sizeof(struct ThreadContext));
  context->lr = (uint32_t) thread_run;

  thread->context = context;
}

void
arch_thread_pop_tf(struct Thread *thread)
{
  asm volatile(
    "mov     sp, %0\n"
    "b       trap_user_exit\n"
    :
    : "r" (thread->tf)
    : "memory"
  );
}
