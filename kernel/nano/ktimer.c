#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/kthread.h>
#include <argentum/spinlock.h>

static unsigned long long tick;
static struct SpinLock tick_lock = SPIN_INITIALIZER("tick");

unsigned long long
ktimer_tick_get(void)
{
  unsigned long long ret;

  spin_lock(&tick_lock);
  ret = tick;
  spin_unlock(&tick_lock);

  return ret;
}

void
ktimer_tick_set(unsigned long long value)
{
  spin_lock(&tick_lock);
  tick = value;
  spin_unlock(&tick_lock);
}

void
ktimer_tick_isr(void)
{
  struct KThread *th = my_thread();

  if (cpu_id() == 0) {
    spin_lock(&tick_lock);
    tick++;
    spin_unlock(&tick_lock);
  }

  if (th != NULL) {
    sched_lock();

    // TODO: check quantum
    th->flags |= KTHREAD_RESCHEDULE;

    sched_unlock();
  }
}
