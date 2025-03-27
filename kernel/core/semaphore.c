#include <kernel/core/config.h>

#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/task.h>

#include "core_private.h"

static int k_semaphore_try_get_locked(struct KSemaphore *);

void
k_semaphore_create(struct KSemaphore *semaphore, int initial_count)
{
  k_spinlock_init(&semaphore->lock, "k_semaphore");
  k_list_init(&semaphore->queue);
  semaphore->type = K_SEMAPHORE_TYPE;
  semaphore->count = initial_count;
}

void
k_semaphore_destroy(struct KSemaphore *semaphore)
{
  k_assert(semaphore != NULL);
  k_assert(semaphore->type == K_SEMAPHORE_TYPE);

  semaphore->type = 0;

  k_spinlock_acquire(&semaphore->lock);
  _k_sched_wakeup_all(&semaphore->queue, K_ERR_INVAL);
  k_spinlock_release(&semaphore->lock);
}

int
k_semaphore_try_get(struct KSemaphore *semaphore)
{
  int r;

  k_assert(semaphore != NULL);
  k_assert(semaphore->type == K_SEMAPHORE_TYPE);

  k_spinlock_acquire(&semaphore->lock);
  r = k_semaphore_try_get_locked(semaphore);
  k_spinlock_release(&semaphore->lock);

  return r;
}

int
k_semaphore_timed_get(struct KSemaphore *semaphore,
                      k_tick_t timeout,
                      int options)
{
  int r;

  k_assert(semaphore != NULL);
  k_assert(semaphore->type == K_SEMAPHORE_TYPE);

  k_spinlock_acquire(&semaphore->lock);

  while ((r = k_semaphore_try_get_locked(semaphore)) < 0) {
    if (r != K_ERR_AGAIN)
      break;

    r = _k_sched_sleep(&semaphore->queue,
                       options & K_SLEEP_UNINTERUPTIBLE
                        ? K_TASK_STATE_SLEEP_UNINTERRUPTIBLE
                        : K_TASK_STATE_SLEEP,
                       timeout,
                       &semaphore->lock);
    if (r < 0)
      break;
  }

  k_spinlock_release(&semaphore->lock);

  return r;
}

static int
k_semaphore_try_get_locked(struct KSemaphore *semaphore)
{
  if (semaphore->count == 0)
    return K_ERR_AGAIN;

  return --semaphore->count;
}

int
k_semaphore_put(struct KSemaphore *semaphore)
{
  k_assert(semaphore != NULL);
  k_assert(semaphore->type == K_SEMAPHORE_TYPE);
  
  k_spinlock_acquire(&semaphore->lock);
  
  semaphore->count++;

  _k_sched_wakeup_one(&semaphore->queue, 0);

  k_spinlock_release(&semaphore->lock);

  return 0;
}
