#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>
#include <kernel/core/task.h>

#include "core_private.h"

void
k_condvar_create(struct KCondVar *cond)
{
  k_list_init(&cond->queue);
  cond->type = K_CONDVAR_TYPE;
  cond->flags = 0;
}

void
k_condvar_destroy(struct KCondVar *cond)
{
  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();
  _k_sched_wakeup_all_locked(&cond->queue, -EINVAL);
  _k_sched_unlock();
}

int
k_condvar_timed_wait(struct KCondVar *cond,
                     struct KMutex *mutex,
                     unsigned long timeout)
{
  int r;

  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  k_assert(k_mutex_holding(mutex));

  _k_sched_lock();

  _k_mutex_unlock(mutex);

  r = _k_sched_sleep(&cond->queue, K_TASK_STATE_SLEEP, timeout, NULL);

  _k_mutex_timed_lock(mutex, 0);

  _k_sched_unlock();

  return r;
}

int
k_condvar_signal(struct KCondVar *cond)
{
  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();

  _k_sched_wakeup_one_locked(&cond->queue, 0);

  _k_sched_unlock();

  return 0;
}

int
k_condvar_broadcast(struct KCondVar *cond)
{
  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);

  _k_sched_lock();
  _k_sched_wakeup_all_locked(&cond->queue, 0);
  _k_sched_unlock();

  return 0;
}
