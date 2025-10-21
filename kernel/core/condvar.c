#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>
#include <kernel/core/task.h>

#include "core_private.h"

// Used by the kernel to verify that the object is a valid conditional variable
#define K_CONDVAR_TYPE    0x434F4E44  // {'C','O','N','D'}

/**
 * @brief Initialize a condition variable.
 *
 * @param cond Pointer to a condition variable object to initialize.
 *
 * @note The condition variable must not be already initialized.
 * @note This function does not allocate dynamic memory.
 */
void
k_condvar_create(struct KCondVar *cond)
{
  k_list_init(&cond->queue);
  cond->type = K_CONDVAR_TYPE;
}

/**
 * @brief Destroy a condition variable and wake up all waiting tasks.
 *
 * This function invalidates the condition variable and wakes all tasks
 * currently waiting on it.
 *
 * @param cond Pointer to a condition variable previously initialized with
 *             `k_condvar_create()`.
 *
 * @note Must not be called while any task holds the associated mutex or
 *       still references the condition variable.
 * @note Any task currently waiting on the semaphore will be woken with
 *       `K_ERR_INVAL`.
 */
void
k_condvar_destroy(struct KCondVar *cond)
{
  k_assert(cond != K_NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();
  _k_sched_wakeup_all_locked(&cond->queue, -K_ERR_INVAL);
  _k_sched_unlock();

  cond->type = 0;

  // TODO: check reschedule
}

/**
 * @brief Wait on a condition variable with an optional timeout.
 *
 * Atomically unlocks the provided mutex and suspends the calling task on the
 * condition variable until it is signaled or the timeout expires.
 *
 * When the function returns (regardless of reason), the mutex is reacquired
 * before returning to the caller.
 *
 * @param cond    Pointer to the condition variable to wait on.
 * @param mutex   Pointer to a mutex currently held by the calling task.
 * @param timeout Timeout duration in system ticks.
 * @param options Wait behavior flags:
 *                - `0`: Normal, interruptible sleep.
 *                - `K_SLEEP_UNWAKEABLE`: Task cannot be interrupted;
 *                  it only wakes on a signal/broadcast or timeout.
 *
 * @retval `0` on successful wakeup (signal or broadcast).
 * @retval `K_ERR_TIMEDOUT` if the timeout expired before a signal was received.
 * @retval `K_ERR_INVAL` if the condition variable was destroyed while waiting.
 *
 * @note The calling task must hold `mutex` before invoking this function.
 * @note Upon return, the mutex is always re-acquired.
 */
int
k_condvar_timed_wait(struct KCondVar *cond,
                     struct KMutex *mutex,
                     k_tick_t timeout,
                     int options)
{
  int r;

  k_assert(cond != K_NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  k_assert(k_mutex_holding(mutex));

  _k_sched_lock();

  _k_mutex_unlock(mutex);

  r = _k_sched_sleep(&cond->queue,
                     options & K_SLEEP_UNWAKEABLE
                      ? K_TASK_STATE_SLEEP_UNWAKEABLE
                      : K_TASK_STATE_SLEEP,
                     timeout,
                     K_NULL);

  _k_mutex_timed_lock(mutex, 0);

  _k_sched_unlock();

  return r;
}

/**
 * @brief Wake one task waiting on a condition variable.
 *
 * Wakes exactly one task currently blocked on the specified condition variable.
 * If no tasks are waiting, this call has no effect.
 *
 * @param cond Pointer to the condition variable to signal.
 *
 * @return 0 on success.
 *
 * @note Typically used when a single waiting task can make progress.
 */
int
k_condvar_notify_one(struct KCondVar *cond)
{
  k_assert(cond != K_NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();
  _k_sched_wakeup_one_locked(&cond->queue, 0);
  _k_sched_unlock();

  // TODO: check reschedule

  return 0;
}

/**
 * @brief Wake all tasks waiting on a condition variable.
 *
 * Wakes all tasks currently blocked on the specified condition variable.
 * If no tasks are waiting, this call has no effect.
 *
 * @param cond Pointer to the condition variable to broadcast.
 *
 * @return 0 on success.
 *
 * @note Typically used when a change in shared state may allow multiple tasks
 *       to proceed.
 */
int
k_condvar_notify_all(struct KCondVar *cond)
{
  k_assert(cond != K_NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();
  _k_sched_wakeup_all_locked(&cond->queue, 0);
  _k_sched_unlock();

  // TODO: check reschedule

  return 0;
}
