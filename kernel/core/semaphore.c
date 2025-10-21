#include <kernel/core/config.h>

#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/task.h>

#include "core_private.h"

// Used by the kernel to verify that the object is a valid semaphore
#define K_SEMAPHORE_TYPE    0x53454D41  // {'S','E','M','A'}

static int k_semaphore_try_get_locked(struct KSemaphore *);

/**
 * @brief Initialize a semaphore object.
 *
 * Creates and initializes a kernel semaphore with a specified initial count.
 *
 * @param sem Pointer to the semaphore object.
 * @param initial_count Initial semaphore count (number of available resources).
 */
void
k_semaphore_create(struct KSemaphore *sem, int initial_count)
{
  k_spinlock_init(&sem->lock, "k_semaphore");
  k_list_init(&sem->queue);
  sem->type = K_SEMAPHORE_TYPE;
  sem->count = initial_count;
}

/**
 * @brief Destroy a semaphore object.
 *
 * Releases all tasks waiting on the semaphore with an error code and marks
 * the semaphore as invalid. After this call, the semaphore must not be used.
 *
 * @param sem Pointer to the semaphore previously initialized with
 *            `k_semaphore_create()`.
 * 
 * @note This function must not be called while any other task may still
 *       reference the semaphore.
 * @note Any task currently waiting on the semaphore will be woken with
 *       `K_ERR_INVAL`.
 */
void
k_semaphore_destroy(struct KSemaphore *sem)
{
  k_assert(sem != K_NULL);
  k_assert(sem->type == K_SEMAPHORE_TYPE);

  k_spinlock_acquire(&sem->lock);
  _k_sched_wakeup_all(&sem->queue, K_ERR_INVAL);
  k_spinlock_release(&sem->lock);

  sem->type = 0;

  // TODO: check reschedule
}

/**
 * @brief Attempt to acquire a semaphore without blocking.
 *
 * Decrements the semaphore count if available. If the count is zero,
 * the call fails immediately.
 *
 * @param sem Pointer to the semaphore to acquire.
 *
 * @retval `>=0` Remaining semaphore count on success.
 * @retval `K_ERR_AGAIN` Resource unavailable and cannot sleep.
 */
int
k_semaphore_try_get(struct KSemaphore *sem)
{
  int r;

  k_assert(sem != K_NULL);
  k_assert(sem->type == K_SEMAPHORE_TYPE);

  k_spinlock_acquire(&sem->lock);
  r = k_semaphore_try_get_locked(sem);
  k_spinlock_release(&sem->lock);

  return r;
}

/**
 * @brief Acquire a semaphore, blocking with optional timeout.
 *
 * If the semaphore count is zero, the calling task is placed into the
 * semaphore’s wait queue and may block until another task releases the
 * semaphore or the specified timeout expires.
 *
 * @param sem Pointer to the semaphore to acquire.
 * @param timeout Timeout duration (in system ticks).
 * @param options Sleeping behavior flags (e.g., `K_SLEEP_UNWAKEABLE`).
 *
 * @retval `>=0` Remaining semaphore count on success.
 * @retval `K_ERR_TIMEOUT` Timeout expired before semaphore became available.
 * @retval `K_ERR_AGAIN` Resource unavailable and cannot sleep.
 * @retval `K_ERR_INTR` Wait was interrupted.
 * 
 * @note This function may cause the calling task to sleep and should be 
 *       called only from a task context.
 */
int
k_semaphore_timed_get(struct KSemaphore *sem,
                      k_tick_t timeout,
                      int options)
{
  int r;

  k_assert(sem != K_NULL);
  k_assert(sem->type == K_SEMAPHORE_TYPE);

  k_spinlock_acquire(&sem->lock);

  while ((r = k_semaphore_try_get_locked(sem)) < 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&sem->queue,
                       options & K_SLEEP_UNWAKEABLE
                        ? K_TASK_STATE_SLEEP_UNWAKEABLE
                        : K_TASK_STATE_SLEEP,
                       timeout,
                       &sem->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&sem->lock);

  return r;
}

static int
k_semaphore_try_get_locked(struct KSemaphore *sem)
{
  k_assert(sem->count >= 0);

  if (sem->count == 0)
    return K_ERR_AGAIN;

  return --sem->count;
}

/**
 * @brief Release (give) a semaphore.
 *
 * Increments the semaphore count and wakes up one waiting task, if any.
 * Can be safely called from within kernel or task context.
 *
 * @param sem Pointer to the semaphore to release.
 *
 * @return Always returns 0.
 * 
 * @note Can be safely called from task, kernel, or ISR context.
 */
int
k_semaphore_put(struct KSemaphore *sem)
{
  k_assert(sem != K_NULL);
  k_assert(sem->type == K_SEMAPHORE_TYPE);
  
  k_spinlock_acquire(&sem->lock);
  
  k_assert(sem->count >= 0);
  sem->count++;

  _k_sched_wakeup_one(&sem->queue, 0);

  k_spinlock_release(&sem->lock);

  // TODO: check reschedule

  return 0;
}
