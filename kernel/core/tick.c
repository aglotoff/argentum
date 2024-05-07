/**
 * @file
 * Kernel ticks
 * 
 * Kernel time is tracked in "ticks" - internal counts in which the kernel
 * processes timeouts and performs thread switch.
 */

#include <kernel/cpu.h>
#include <kernel/tick.h>
#include <kernel/ktimer.h>
#include <kernel/spin.h>
#include <kernel/thread.h>

static struct SpinLock tick_lock = SPIN_INITIALIZER("tick");
static unsigned long tick_counter;

/**
 * Notify the kernel that a tick occured.
 */
void
tick(void)
{
  struct Thread *current_task = thread_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for other sheduling policies
  if (current_task != NULL) {
    sched_lock();
    current_task->flags |= THREAD_FLAG_RESCHEDULE;
    sched_unlock();
  }

  // TODO: all timeouts are processed by CPU #0, is it ok?
  if (cpu_id() == 0) {
    // Increment the tick counter
    spin_lock(&tick_lock);
    tick_counter++;
    spin_unlock(&tick_lock);

    // Process timeouts
    ktimer_tick();
  }
}

/**
 * Get the current value of the tick counter.
 */
unsigned long
tick_get(void)
{
  unsigned long counter;

  // TODO: could use atomic operation
  spin_lock(&tick_lock);
  counter = tick_counter;
  spin_unlock(&tick_lock);

  return counter;
}
