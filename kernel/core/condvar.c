#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/condvar.h>
#include <kernel/core/mutex.h>
#include <kernel/core/task.h>
#include <kernel/object_pool.h>

#include "core_private.h"

static void k_condvar_ctor(void *, size_t);
static void k_condvar_dtor(void *, size_t);
static void k_condvar_init_common(struct KCondVar *);
static void k_condvar_fini_common(struct KCondVar *);

static struct KObjectPool *k_condvar_pool;

void
k_condvar_system_init(void)
{
  k_condvar_pool = k_object_pool_create("k_condvar",
                                        sizeof(struct KCondVar),
                                        0,
                                        k_condvar_ctor,
                                        k_condvar_dtor);
  if (k_condvar_pool == NULL)
    k_panic("cannot create the condvar pool");
}

void
k_condvar_init(struct KCondVar *cond)
{
  k_condvar_ctor(cond, sizeof(struct KCondVar));
  k_condvar_init_common(cond);
  cond->flags = K_CONDVAR_STATIC;
}

struct KCondVar *
k_condvar_create(void)
{
  struct KCondVar *cond;

  if ((cond = (struct KCondVar *) k_object_pool_get(k_condvar_pool)) == NULL)
    return NULL;

  k_condvar_init_common(cond);
  cond->flags = 0;

  return cond;
}

static void
k_condvar_init_common(struct KCondVar *)
{
  // TODO
}

void
k_condvar_fini(struct KCondVar *cond)
{
  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);
  k_assert(cond->flags & K_CONDVAR_STATIC);

  k_condvar_fini_common(cond);
}

void
k_condvar_destroy(struct KCondVar *cond)
{
  k_assert(cond != NULL);
  k_assert(cond->type == K_CONDVAR_TYPE);
  k_assert(!(cond->flags & K_CONDVAR_STATIC));

  k_condvar_fini_common(cond);

  k_object_pool_put(k_condvar_pool, cond);
}

static void
k_condvar_fini_common(struct KCondVar *cond)
{
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

static void
k_condvar_ctor(void *p, size_t n)
{
  struct KCondVar *cond = (struct KCondVar *) p;
  (void) n;

  k_list_init(&cond->queue);
  cond->type = K_CONDVAR_TYPE;
}

static void
k_condvar_dtor(void *p, size_t n)
{
  struct KCondVar *cond = (struct KCondVar *) p;
  (void) n;

  k_assert(!k_list_is_empty(&cond->queue));
}
