#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/kthread.h>
#include <argentum/spinlock.h>

static unsigned long long tick;
static struct SpinLock tick_lock = SPIN_INITIALIZER("tick");

unsigned long long
ktimer_get_tick(void)
{
  unsigned long long ret;

  spin_lock(&tick_lock);
  ret = tick;
  spin_unlock(&tick_lock);

  return ret;
}

void
ktimer_set_tick(unsigned long long value)
{
  spin_lock(&tick_lock);
  tick = value;
  spin_unlock(&tick_lock);
}

/**
 * Notify the kernel that a timer IRQ has occured.
 */
void
ktimer_tick_isr(void)
{
  struct KThread *current_thread = kthread_current();

  // On BSP, update the dynamic tick counter
  // TODO: update timeouts
  if (cpu_id() == 0) {
    spin_lock(&tick_lock);
    tick++;
    spin_unlock(&tick_lock);
  }

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for more complex sheculing policies
  if (current_thread != NULL) {
    sched_lock();
    current_thread->flags |= KTHREAD_RESCHEDULE;
    sched_unlock();
  }
}
