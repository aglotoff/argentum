#include <kernel/cpu.h>
#include <kernel/ktime.h>
#include <kernel/ktimer.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>

static struct SpinLock ktime_lock = SPIN_INITIALIZER("ktime");
static unsigned long ktime_ticks;

/**
 * Notify the kernel that a timer IRQ has occured.
 */
void
ktime_tick(void)
{
  struct Thread *current_task = thread_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for more complex sheculing policies
  if (current_task != NULL) {
    sched_lock();
    current_task->flags |= THREAD_FLAG_RESCHEDULE;
    sched_unlock();
  }

  if (cpu_id() == 0) {
    spin_lock(&ktime_lock);
    ktime_ticks++;
    spin_unlock(&ktime_lock);

    ktimer_tick();
  }
}



unsigned long
ktime_get(void)
{
  unsigned long ticks;

  spin_lock(&ktime_lock);
  ticks = ktime_ticks;
  spin_unlock(&ktime_lock);

  return ticks;
}
