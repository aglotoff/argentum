#include <kernel/assert.h>
#include <errno.h>

#include <kernel/core/cpu.h>
#include <kernel/core/semaphore.h>
#include <kernel/task.h>
#include <kernel/object_pool.h>

#include "core_private.h"

static void k_semaphore_ctor(void *, size_t);
static void k_semaphore_dtor(void *, size_t);
static void k_semaphore_init_common(struct KSemaphore *, int);
static void k_semaphore_fini_common(struct KSemaphore *);
static int  k_semaphore_try_get_locked(struct KSemaphore *);

static struct KObjectPool *k_semaphore_pool;

void
k_semaphore_system_init(void)
{
  k_semaphore_pool = k_object_pool_create("k_semaphore",
                                          sizeof(struct KSemaphore),
                                          0,
                                          k_semaphore_ctor,
                                          k_semaphore_dtor);
  if (k_semaphore_pool == NULL)
    panic("cannot create the semaphore pool");
}

void
k_semaphore_init(struct KSemaphore *semaphore, int initial_count)
{
  k_semaphore_ctor(semaphore, sizeof(struct KSemaphore));
  k_semaphore_init_common(semaphore, initial_count);
  semaphore->flags = K_SEMAPHORE_STATIC;
}

struct KSemaphore *
k_semaphore_create(int initial_count)
{
  struct KSemaphore *semaphore;

  if ((semaphore = (struct KSemaphore *) k_object_pool_get(k_semaphore_pool)) == NULL)
    return NULL;

  k_semaphore_init_common(semaphore, initial_count);
  semaphore->flags = 0;

  return semaphore;
}

static void
k_semaphore_init_common(struct KSemaphore *semaphore, int initial_count)
{
  semaphore->count = initial_count;
}

void
k_semaphore_fini(struct KSemaphore *semaphore)
{
  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    panic("bad semaphore pointer");
  if (!(semaphore->flags & K_SEMAPHORE_STATIC))
    panic("cannot fini non-static semaphores");

  k_semaphore_fini_common(semaphore);
}

void
k_semaphore_destroy(struct KSemaphore *semaphore)
{
  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    panic("bad semaphore pointer");
  if (semaphore->flags & K_SEMAPHORE_STATIC)
    panic("cannot destroy static semaphores");

  k_semaphore_fini_common(semaphore);

  k_object_pool_put(k_semaphore_pool, semaphore);
}

static void
k_semaphore_fini_common(struct KSemaphore *semaphore)
{
  k_spinlock_acquire(&semaphore->lock);
  _k_sched_wakeup_all(&semaphore->queue, -EINVAL);
  k_spinlock_release(&semaphore->lock);
}

int
k_semaphore_try_get(struct KSemaphore *semaphore)
{
  int r;

  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    panic("bad semaphore pointer");

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
    panic("bad semaphore pointer");

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
  assert(semaphore->count >= 0);

  if (semaphore->count == 0)
    return -EAGAIN;

  return --semaphore->count;
}

int
k_semaphore_put(struct KSemaphore *semaphore)
{
  if ((semaphore == NULL) || (semaphore->type != K_SEMAPHORE_TYPE))
    panic("bad semaphore pointer");
  
  k_spinlock_acquire(&semaphore->lock);
  
  semaphore->count++;

  _k_sched_wakeup_one(&semaphore->queue, 0);

  k_spinlock_release(&semaphore->lock);

  return 0;
}

static void
k_semaphore_ctor(void *p, size_t n)
{
  struct KSemaphore *semaphore = (struct KSemaphore *) p;
  (void) n;

  k_spinlock_init(&semaphore->lock, "k_semaphore");
  k_list_init(&semaphore->queue);
  semaphore->type = K_SEMAPHORE_TYPE;
}

static void
k_semaphore_dtor(void *p, size_t n)
{
  struct KSemaphore *semaphore = (struct KSemaphore *) p;
  (void) n;

  assert(!k_list_is_empty(&semaphore->queue));
}
