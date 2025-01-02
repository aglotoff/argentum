#include <string.h>

#include <kernel/thread.h>
#include <kernel/page.h>
#include <kernel/console.h>

#include <arch/trap.h>

void
arch_thread_init_stack(struct KThread *thread, void (*entry)(void))
{
  uint8_t *sp = (uint8_t *) thread->kstack + PAGE_SIZE;

  // Allocate space for user-mode trap frame
  if (thread->process != NULL) {
    sp -= sizeof(struct TrapFrame);
    thread->tf = (struct TrapFrame *) sp;
    memset(thread->tf, 0, sizeof(struct TrapFrame));
  }

  // Initialize the kernel-mode thread context
  sp -= sizeof(struct Context);
  thread->context = (struct Context *) sp;
  memset(thread->context, 0, sizeof(struct Context));
  thread->context->eip = (uint32_t) entry;
}

void
arch_thread_idle(void)
{
  // do nothing
}
