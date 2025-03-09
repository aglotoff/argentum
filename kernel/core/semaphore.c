#include <kernel/core/assert.h>
#include <errno.h>

#include <kernel/core/cpu.h>
#include <kernel/core/semaphore.h>
#include <kernel/core/task.h>

#include "core_private.h"

static int  k_semaphore_try_get_locked(struct KSemaphore *);

void
k_semaphore_create(struct KSemaphore *semaphore, int initial_count)
{
  k_spinlock_init(&semaphore->lock, "k_semaphore");
  k_list_init(&semaphore->queue);
  semaphore->type = K_SEMAPHORE_TYPE;
  semaphore->count = initial_count;
  semaphore->flags = 0;
}

void
k_semaphore_destroy(struct KSemaphore *semaphore)
{
  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    k_panic("bad semaphore pointer");

  k_spinlock_acquire(&semaphore->lock);
  _k_sched_wakeup_all(&semaphore->queue, -EINVAL);
  k_spinlock_release(&semaphore->lock);
}

int
k_semaphore_try_get(struct KSemaphore *semaphore)
{
  int r;

  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    k_panic("bad semaphore pointer");

  k_spinlock_acquire(&semaphore->lock);
  r = k_semaphore_try_get_locked(semaphore);
  k_spinlock_release(&semaphore->lock);

  return r;
}

int
k_semaphore_timed_get(struct KSemaphore *semaphore, unsigned long timeout)
{
  int r;

  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    k_panic("bad semaphore pointer");

  k_spinlock_acquire(&semaphore->lock);

  while ((r = k_semaphore_try_get_locked(semaphore)) != 0) {
    if (r != -EAGAIN)
      break;

    r = _k_sched_sleep(&semaphore->queue,
                       K_TASK_STATE_SLEEP,
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
  k_assert(semaphore->count >= 0);

  if (semaphore->count == 0)
    return -EAGAIN;

  return --semaphore->count;
}

int
k_semaphore_put(struct KSemaphore *semaphore)
{
  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    k_panic("bad semaphore pointer");
  
  k_spinlock_acquire(&semaphore->lock);
  
  semaphore->count++;

  _k_sched_wakeup_one(&semaphore->queue, 0);

  k_spinlock_release(&semaphore->lock);

  return 0;
}
