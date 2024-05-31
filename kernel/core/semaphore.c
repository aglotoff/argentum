#include <kernel/assert.h>
#include <errno.h>

#include <kernel/cpu.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>

#include "core_private.h"

int
k_semaphore_create(struct KSemaphore *sem, unsigned long initial_count)
{
  list_init(&sem->queue);
  sem->count = initial_count;

  return 0;
}

int
k_semaphore_destroy(struct KSemaphore *sem)
{
  _k_sched_lock();
  _k_sched_wakeup_all(&sem->queue, -EINVAL);
  _k_sched_unlock();

  return 0;
}

int
k_semaphore_get(struct KSemaphore *sem, unsigned long timeout, int blocking)
{
  struct KThread *thread = k_thread_current();
  int r;

  if ((thread == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  _k_sched_lock();

  while (sem->count == 0) {
    struct KCpu *my_cpu = k_cpu();

    if (!blocking || (my_cpu->isr_nesting > 0)) {
      // Can't block
      _k_sched_unlock();
      return -EAGAIN;
    }

    if ((r = _k_sched_sleep(&sem->queue, 0, timeout, NULL)) != 0) {
      _k_sched_unlock();
      return r;
    }
  }

  r = --sem->count;

  _k_sched_unlock();
  return r;
}

int
k_semaphore_put(struct KSemaphore *sem)
{
  _k_sched_lock();

  sem->count++;
  _k_sched_wakeup_one(&sem->queue, 0);

  _k_sched_unlock();

  return 0;
}
