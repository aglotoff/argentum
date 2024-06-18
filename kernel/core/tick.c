/**
 * @file
 * Kernel ticks
 * 
 * Kernel time is tracked in "ticks" - internal counts in which the kernel
 * processes timeouts and performs thread switch.
 */

#include <kernel/cpu.h>
#include <kernel/tick.h>
#include <kernel/timer.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>

#include "core_private.h"

static struct KSpinLock tick_lock = K_SPINLOCK_INITIALIZER("tick");
static unsigned long tick_counter;

/**
 * Notify the kernel that a tick occured.
 */
void
tick(void)
{
  struct KThread *current_task = k_thread_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for other sheduling policies
  if (current_task != NULL) {
    _k_sched_spin_lock();
    current_task->flags |= THREAD_FLAG_RESCHEDULE;
    _k_sched_spin_unlock();
  }

  // TODO: all timeouts are processed by CPU #0, is it ok?
  if (k_cpu_id() == 0) {
    // Increment the tick counter
    k_spinlock_acquire(&tick_lock);
    tick_counter++;
    k_spinlock_release(&tick_lock);

    // Process timeouts
    k_timer_tick();
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
  k_spinlock_acquire(&tick_lock);
  counter = tick_counter;
  k_spinlock_release(&tick_lock);

  return counter;
}
