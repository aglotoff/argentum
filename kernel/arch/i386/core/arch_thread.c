#include <string.h>

#include <kernel/thread.h>
#include <kernel/page.h>

void
arch_thread_init_stack(struct KThread *thread, void (*entry)(void))
{
  // TODO
  (void) thread;
  (void) entry;
}

void
arch_thread_idle(void)
{
  // TODO
}
