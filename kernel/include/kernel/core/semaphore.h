#ifndef __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__

#include <kernel/core/config.h>
#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/types.h>

/**
 * @brief Kernel counting semaphore object.
 *
 * A semaphore is a synchronization primitive that allows a fixed number of
 * concurrent holders. It maintains an internal count representing the number
 * of available "tokens." Tasks attempting to acquire the semaphore will block
 * when the count reaches zero and will be queued until another task releases
 * it.
 */
struct KSemaphore {
  int type;
  struct KSpinLock lock;
  unsigned long count;
  struct KListLink queue;
};

void k_semaphore_create(struct KSemaphore *, int);
void k_semaphore_destroy(struct KSemaphore *);
int k_semaphore_try_get(struct KSemaphore *);
int k_semaphore_timed_get(struct KSemaphore *, k_tick_t, int);
int k_semaphore_put(struct KSemaphore *);

/**
 * @brief Acquire a semaphore.
 *
 * If the semaphore count is zero, the calling task is placed into the
 * semaphore’s wait queue and may block until another task releases the
 * semaphore.
 *
 * @param sem Pointer to the semaphore to acquire.
 * @param options Sleeping behavior flags (e.g., `K_SLEEP_UNWAKEABLE`).
 *
 * @retval `>=0` Remaining semaphore count on success.
 * @retval `K_ERR_INTR` Wait was interrupted.
 * 
 * @note This function may cause the calling task to sleep and should be 
 *       called only from a task context.
 */
static inline int
k_semaphore_get(struct KSemaphore *semaphore, int options)
{
  return k_semaphore_timed_get(semaphore, 0, options);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
