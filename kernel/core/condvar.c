#include <kernel/assert.h>
#include <errno.h>

#include <kernel/core/cpu.h>
#include <kernel/core/condvar.h>
#include <kernel/mutex.h>
#include <kernel/task.h>
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
    panic("cannot create the condvar pool");
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
  if ((cond == NULL) || (cond->type != K_CONDVAR_TYPE))
    panic("bad cond pointer");
  if (!(cond->flags & K_CONDVAR_STATIC))
    panic("cannot fini non-static condvars");

  k_condvar_fini_common(cond);
}

void
k_condvar_destroy(struct KCondVar *cond)
{
  if ((cond == NULL) || (cond->type != K_CONDVAR_TYPE))
    panic("bad cond pointer");
  if (cond->flags & K_CONDVAR_STATIC)
    panic("cannot destroy static condvars");

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
k_condvar_timed_wait(struct KCondVar *cond, struct KMutex *mutex, unsigned long timeout)
{
  int r;

  if ((cond == NULL) || (cond->type != K_CONDVAR_TYPE))
    panic("bad condvar pointer");

  if (!k_mutex_holding(mutex))
    panic("not holding");

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
  if ((cond == NULL) || (cond->type != K_CONDVAR_TYPE))
    panic("bad condvar pointer");
  
  _k_sched_lock();

  _k_sched_wakeup_one_locked(&cond->queue, 0);

  _k_sched_unlock();

  return 0;
}

int
k_condvar_broadcast(struct KCondVar *cond)
{
  if ((cond == NULL) || (cond->type != K_CONDVAR_TYPE))
    panic("bad condvar pointer");
  
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

  assert(!k_list_is_empty(&cond->queue));
}
