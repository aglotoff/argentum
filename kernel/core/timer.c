#include <kernel/assert.h>
#include <errno.h>

#include <kernel/console.h>
#include <kernel/timer.h>
#include <kernel/spinlock.h>

#include "core_private.h"

static void k_timer_enqueue(struct KTimer *, unsigned long);
static void k_timer_dequeue(struct KTimer *);

static KLIST_DECLARE(k_timer_queue);
static struct KSpinLock k_timer_lock = K_SPINLOCK_INITIALIZER("k_timer");
static struct KTimer *k_timer_current;

int
k_timer_init(struct KTimer *timer,
             void (*callback)(void *),
             void *callback_arg,
             unsigned long delay,
             unsigned long period,
             int autostart)
{
  if (timer == NULL)
    panic("timer is NULL");

  _k_timeout_init(&timer->entry);
  
  timer->callback = callback;
  timer->callback_arg = callback_arg;
  timer->delay  = delay;
  timer->period = period;

  // TODO: validate delay and period!

  if (autostart) {
    k_spinlock_acquire(&k_timer_lock);
    k_timer_enqueue(timer, delay);
    k_spinlock_release(&k_timer_lock);
  }

  return 0;
}

void
_k_timer_start(struct KTimer *timer, unsigned long remain)
{
  k_spinlock_acquire(&k_timer_lock);
  k_timer_enqueue(timer, remain);
  k_spinlock_release(&k_timer_lock);
}

int
k_timer_start(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_spinlock_acquire(&k_timer_lock);

  if (!k_list_is_null(timer->entry.link.next)) {
    k_spinlock_release(&k_timer_lock);
    return -EINVAL;
  }

  k_timer_enqueue(timer, timer->delay);

  k_spinlock_release(&k_timer_lock);

  return 0;
}

int
k_timer_stop(struct KTimer *timer)
{
  if (timer == NULL)
    panic("timer is NULL");

  k_spinlock_acquire(&k_timer_lock);

  if (!k_list_is_null(&timer->entry.link)) {
    k_timer_dequeue(timer);
  } else if (k_timer_current == timer) {
    k_timer_current = NULL;
  }

  k_spinlock_release(&k_timer_lock);

  return 0;
}

int
k_timer_fini(struct KTimer *timer)
{
  return k_timer_stop(timer);
}

void
_k_timer_timeout(struct KTimeout *entry)
{
  struct KTimer *timer = (struct KTimer *) entry;

  void (*callback)(void *) = timer->callback;
  void *callback_arg = timer->callback_arg;

  k_timer_current = timer;

  k_spinlock_release(&k_timer_lock);

  callback(callback_arg);

  k_spinlock_acquire(&k_timer_lock);

  if (k_timer_current != NULL) {
    assert(k_timer_current == timer);

    if (timer->period != 0)
      k_timer_enqueue(timer, timer->period);

    k_timer_current = NULL;
  }
}

void
k_timer_tick(void)
{
  k_spinlock_acquire(&k_timer_lock);
  _k_timeout_process_queue(&k_timer_queue, _k_timer_timeout);
  k_spinlock_release(&k_timer_lock);
}

static void
k_timer_enqueue(struct KTimer *timer, unsigned long delay)
{
  assert(k_spinlock_holding(&k_timer_lock));
  _k_timeout_enqueue(&k_timer_queue, &timer->entry, delay);
}

static void
k_timer_dequeue(struct KTimer *timer)
{
  assert(k_spinlock_holding(&k_timer_lock));
  _k_timeout_dequeue(&k_timer_queue, &timer->entry);
}
