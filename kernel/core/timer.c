#include <kernel/core/assert.h>
#include <kernel/core/timer.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

// Used by the kernel to verify that the object is a valid timer
#define K_TIMER_TYPE  0x54494D52  // {'T','I','M','R'}

enum {
  K_TIMER_STATE_NONE     = 0,
  K_TIMER_STATE_ACTIVE   = 1,
  K_TIMER_STATE_INACTIVE = 2,
  K_TIMER_STATE_RUNNING  = 3,
};

static void k_timer_enqueue(struct KTimer *, k_tick_t);
static void k_timer_dequeue(struct KTimer *);

static K_LIST_DECLARE(k_timer_queue);
static struct KSpinLock k_timer_lock = K_SPINLOCK_INITIALIZER("k_timer");
static struct KTimer *k_timer_current;

/**
 * @brief Create and optionally start a kernel timer.
 *
 * Initializes a timer object with the specified callback, delay, and period.
 * The timer can be configured as one-shot (period = 0) or periodic
 * (period > 0).
 *
 * @param timer        Pointer to the timer object to initialize.
 * @param callback     Function to be invoked when the timer expires.
 * @param callback_arg Argument passed to the callback function.
 * @param delay        Initial delay before the first expiration (in ticks).
 * @param period       Period between successive expirations (in ticks), or 0
 *                     for one-shot.
 *
 * @retval 0 on success.
 *
 * @note Timer creation does not allocate memory; the caller provides storage.
 */
int
k_timer_create(struct KTimer *timer,
               void (*callback)(void *),
               void *callback_arg,
               k_tick_t delay,
               k_tick_t period)
{
  k_assert(timer != K_NULL);

  _k_timeout_create(&timer->queue_entry);
  
  timer->callback = callback;
  timer->callback_arg = callback_arg;
  timer->delay = delay;
  timer->period = period;
  timer->type = K_TIMER_TYPE;

  return 0;
}

/**
 * @brief Destroy a kernel timer.
 *
 * Stops the timer if it is active and marks it as invalid.
 *
 * @param timer Pointer to the timer object.
 *
 * @retval 0 on success.
 *
 * @note All active timer callbacks are guaranteed to complete before
 *       the timer is destroyed.
 * @note After destruction, the timer object must not be reused without
 *       reinitialization via `k_timer_create()`.
 */
int
k_timer_destroy(struct KTimer *timer)
{
  k_timer_stop(timer);
  timer->type = 0;
  return 0;
}

void
_k_timer_start(struct KTimer *timer, k_tick_t remain)
{
  k_spinlock_acquire(&k_timer_lock);
  k_timer_enqueue(timer, remain);
  k_spinlock_release(&k_timer_lock);
}

/**
 * @brief Start a timer.
 *
 * Adds the timer to the system timer queue using its configured delay.
 * If the timer is already active, this call fails.
 *
 * @param timer Pointer to the timer object.
 *
 * @retval `0` on success.
 * @retval `K_ERR_INVAL` if the timer is already running.
 */
int
k_timer_start(struct KTimer *timer)
{
  k_assert(timer != K_NULL);
  k_assert(timer->type == K_TIMER_TYPE);

  k_spinlock_acquire(&k_timer_lock);

  if (!k_list_is_null(timer->queue_entry.link.next)) {
    k_spinlock_release(&k_timer_lock);
    return K_ERR_INVAL;
  }

  k_timer_enqueue(timer, timer->delay);

  k_spinlock_release(&k_timer_lock);

  return 0;
}

/**
 * @brief Stop a running timer.
 *
 * Removes the timer from the active timer queue. If the timer is currently
 * executing its callback, it is marked as stopped and will not be rescheduled.
 *
 * @param timer Pointer to the timer object.
 *
 * @retval 0 on success.
 */
int
k_timer_stop(struct KTimer *timer)
{
  k_assert(timer != K_NULL);
  k_assert(timer->type == K_TIMER_TYPE);

  k_spinlock_acquire(&k_timer_lock);

  if (!k_list_is_null(&timer->queue_entry.link)) {
    k_timer_dequeue(timer);
  } else if (k_timer_current == timer) {
    k_timer_current = K_NULL;
  }

  k_spinlock_release(&k_timer_lock);

  return 0;
}

void
_k_timer_timeout(struct KTimeoutEntry *entry)
{
  struct KTimer *timer = (struct KTimer *) entry;

  k_assert(k_spinlock_holding(&k_timer_lock));

  void (*callback)(void *) = timer->callback;
  void *callback_arg = timer->callback_arg;

  k_timer_current = timer;

  k_spinlock_release(&k_timer_lock);

  callback(callback_arg);

  k_spinlock_acquire(&k_timer_lock);

  if (k_timer_current != K_NULL) {
    k_assert(k_timer_current == timer);

    if (timer->period != 0)
      k_timer_enqueue(timer, timer->period);

    k_timer_current = K_NULL;
  }
}

void
_k_timer_tick(void)
{
  k_spinlock_acquire(&k_timer_lock);
  _k_timeout_process_queue(&k_timer_queue, _k_timer_timeout);
  k_spinlock_release(&k_timer_lock);
}

static void
k_timer_enqueue(struct KTimer *timer, k_tick_t delay)
{
  k_assert(k_spinlock_holding(&k_timer_lock));
  _k_timeout_enqueue(&k_timer_queue, &timer->queue_entry, delay);
}

static void
k_timer_dequeue(struct KTimer *timer)
{
  k_assert(k_spinlock_holding(&k_timer_lock));
  _k_timeout_dequeue(&k_timer_queue, &timer->queue_entry);
}
