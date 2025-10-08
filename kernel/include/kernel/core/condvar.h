#ifndef __INCLUDE_KENEL_CORE_CONDVAR_H__
#define __INCLUDE_KENEL_CORE_CONDVAR_H__

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/types.h>

/**
 * @brief Kernel condition variable object.
 *
 * Condition variables allow tasks to wait (block) until a particular condition
 * becomes true. They are typically used in combination with mutexes to
 * synchronize access to shared resources.
 *
 * Each condition variable maintains an internal wait queue of tasks that are
 * suspended pending a notify event.
 */
struct KCondVar {
  int type;
  struct KListLink queue;
};

struct KMutex;

void k_condvar_create(struct KCondVar *);
void k_condvar_destroy(struct KCondVar *);
int k_condvar_timed_wait(struct KCondVar *, struct KMutex *, k_tick_t, int);
int k_condvar_notify_one(struct KCondVar *);
int k_condvar_notify_all(struct KCondVar *);

/**
 * @brief Wait on a condition variable.
 *
 * Atomically unlocks the provided mutex and suspends the calling task on the
 * condition variable until it is signaled.
 *
 * When the function returns (regardless of reason), the mutex is reacquired
 * before returning to the caller.
 *
 * @param cond    Pointer to the condition variable to wait on.
 * @param mutex   Pointer to a mutex currently held by the calling task.
 * @param options Wait behavior flags that control sleep characteristics:
 *                - `0`: Normal, interruptible sleep.
 *                - `K_SLEEP_UNWAKEABLE`: Task cannot be interrupted;
 *                  it only wakes on a signal/broadcast or timeout.
 *
 * @return 0 on successful wakeup (signal or broadcast).
 * @return -EINVAL if the condition variable was destroyed while waiting.
 *
 * @note The calling task must hold `mutex` before invoking this function.
 * @note Upon return, the mutex is always re-acquired.
 */
static inline int
k_condvar_wait(struct KCondVar *cond, struct KMutex *mutex, int options)
{
  return k_condvar_timed_wait(cond, mutex, 0, options);
}

#endif  // !__INCLUDE_KENEL_CORE_CONDVAR_H__
