#include <string.h>

#include <kernel/page.h>
#include <kernel/thread.h>

void
arch_thread_idle(void)
{
  asm volatile("wfi");
}

void
arch_thread_create(struct Thread *thread)
{
  uint8_t *sp = (uint8_t *) thread->kstack + PAGE_SIZE;
  struct ThreadContext *context;

  sp -= sizeof(struct ThreadContext);
  context = (struct ThreadContext *) sp;
  memset(context, 0, sizeof(struct ThreadContext));
  context->lr = (uint32_t) thread_run;

  thread->context = context;
}
