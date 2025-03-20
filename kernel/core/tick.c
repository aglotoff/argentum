/**
 * @file
 * Kernel ticks
 * 
 * Kernel time is tracked in "ticks" - internal counts in which the kernel
 * processes timeouts and performs task switch.
 */

#include <kernel/core/cpu.h>
#include <kernel/core/timer.h>
#include <kernel/core/tick.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/task.h>

#include "core_private.h"

static struct KSpinLock k_tick_lock = K_SPINLOCK_INITIALIZER("k_tick");
static unsigned long long k_tick_counter = 0;

/**
 * Notify the kernel that a tick occured.
 */
void
k_timer_tick(void)
{
  _k_sched_timer_tick();
  _k_timer_tick();

  // TODO: all timeouts are processed by CPU #0, is it ok?
  k_spinlock_acquire(&k_tick_lock);
  k_tick_counter++;
  k_spinlock_release(&k_tick_lock);
}

/**
 * Get the current value of the tick counter.
 */
unsigned long long
k_tick_get(void)
{
  unsigned long long counter;

  k_spinlock_acquire(&k_tick_lock);
  counter = k_tick_counter;
  k_spinlock_release(&k_tick_lock);

  return counter;
}

void
k_tick_set(unsigned long long counter)
{
  // TODO: update timeouts
  k_spinlock_acquire(&k_tick_lock);
  k_tick_counter = counter;
  k_spinlock_release(&k_tick_lock);
}
