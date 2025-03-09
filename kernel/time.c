#include <sys/types.h>
#include <errno.h>

#include <kernel/core/assert.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/tick.h>
#include <kernel/process.h>
#include <kernel/console.h>
#include <kernel/time.h>

static unsigned long long skip_ticks = 0;
static unsigned ticks_to_sync = 0;

#define TICKS_SYNC_PERIOD   TICKS_PER_SECOND

void
time_init(void)
{
  arch_time_init();

  if (k_cpu_id() == 0) {
    k_tick_set(seconds2ticks(arch_get_time_seconds()));
    ticks_to_sync = TICKS_SYNC_PERIOD;
  }
}

time_t
time_get_seconds(void)
{
  return ticks2seconds(k_tick_get());
}

void
time_tick(void)
{
  if (k_cpu_id() == 0) {
    ticks_to_sync--;

    if (skip_ticks > 0) {
      skip_ticks--;
      return;
    }

    if (ticks_to_sync == 0) {
      unsigned long expected_ticks = seconds2ticks(arch_get_time_seconds());
      unsigned long current_ticks = k_tick_get();

      // TODO: panic if difference gets too big?

      if (current_ticks < expected_ticks) {
        k_tick_set(expected_ticks);
      } else if (current_ticks > expected_ticks) {
        skip_ticks = current_ticks - expected_ticks;
      }

      ticks_to_sync = TICKS_SYNC_PERIOD;
    }
  }
}

int
time_get(clockid_t clock_id, struct timespec *tp)
{
  if (tp == NULL)
    k_panic("tp is null");

  if ((clock_id != CLOCK_REALTIME) && (clock_id != CLOCK_MONOTONIC))
    return -EINVAL;

  ticks2timespec(k_tick_get(), tp);

  return 0;
}

int
time_nanosleep(struct timespec *rqtp, struct timespec *rmtp)
{
  unsigned long long req_ticks, elapsed_ticks;
  int r;

  if ((rqtp->tv_nsec < 0) || (rqtp->tv_nsec >= 1000000000L))
    return -EINVAL;

  req_ticks = timespec2ticks(rqtp);
  
  if (req_ticks == 0) {
    elapsed_ticks = 0;
    r = 0;
  } else {
    unsigned long start_ticks = k_tick_get();
    struct KSemaphore sem;

    k_semaphore_create(&sem, 0);
    r = k_semaphore_timed_get(&sem, req_ticks);

    elapsed_ticks = MIN(k_tick_get() - start_ticks, req_ticks);

    k_semaphore_destroy(&sem);
  }

  if (rmtp != NULL)
    ticks2timespec(elapsed_ticks, rmtp);

  return r == -ETIMEDOUT ? 0 : r;
}

int
timer_irq(int, void *)
{
  struct Process *my_process = process_current();

  if (my_process != NULL) {
    k_assert(my_process->thread != NULL);

    if (arch_trap_is_user(my_process->thread->tf)) {
      process_update_times(my_process, 1, 0);
    } else {
      process_update_times(my_process, 0, 1);
    }
  }

  k_tick();

  time_tick();

  return 1;
}
