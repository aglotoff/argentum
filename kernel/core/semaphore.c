#include <kernel/assert.h>
#include <errno.h>

#include <kernel/cpu.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>
#include <kernel/object_pool.h>

#include "core_private.h"

static void k_semaphore_ctor(void *, size_t);
static void k_semaphore_dtor(void *, size_t);

static struct KObjectPool *k_semaphore_pool;

void
k_semaphore_system_init(void)
{
  k_semaphore_pool = k_object_pool_create("semaphore",
                                          sizeof(struct KSemaphore),
                                          0,
                                          k_semaphore_ctor,
                                          k_semaphore_dtor);
  if (k_semaphore_pool == NULL)
    panic("cannot create the semaphore pool");
}

void
k_semaphore_init(struct KSemaphore *sem, int initial_count)
{
  k_semaphore_ctor(sem, sizeof(struct KSemaphore));

  sem->count = initial_count;
  sem->flags = KSEMAPHORE_STATIC;
}

void
k_semaphore_fini(struct KSemaphore *sem)
{
  k_spinlock_acquire(&sem->lock);
  _k_sched_wakeup_all(&sem->queue, -EINVAL);
  k_spinlock_release(&sem->lock);
}

struct KSemaphore *
k_semaphore_create(int initial_count)
{
  struct KSemaphore *sem;

  if ((sem = (struct KSemaphore *) k_object_pool_get(k_semaphore_pool)) == NULL)
    return NULL;

  sem->count = initial_count;
  sem->flags = 0;

  return sem;
}

void
k_semaphore_destroy(struct KSemaphore *sem)
{
  if (sem->flags & KSEMAPHORE_STATIC)
    panic("destroying a static semaphore");

  k_semaphore_fini(sem);
  k_object_pool_put(k_semaphore_pool, sem);
}

int
k_semaphore_timed_get(struct KSemaphore *sem, unsigned long timeout)
{
  int r;

  k_spinlock_acquire(&sem->lock);

  while (sem->count == 0) {
    if ((r = _k_sched_sleep(&sem->queue, 0, timeout, &sem->lock)) < 0) {
      k_spinlock_release(&sem->lock);
      return r;
    }
  }

  r = --sem->count;

  k_spinlock_release(&sem->lock);

  return r;
}

int
k_semaphore_put(struct KSemaphore *sem)
{
  k_spinlock_acquire(&sem->lock);
  
  sem->count++;

  _k_sched_wakeup_one(&sem->queue, 0);

  k_spinlock_release(&sem->lock);

  return 0;
}

static void
k_semaphore_ctor(void *p, size_t n)
{
  struct KSemaphore *sem = (struct KSemaphore *) p;
  (void) n;

  k_spinlock_init(&sem->lock, "k_semaphore");
  k_list_init(&sem->queue);
}

static void
k_semaphore_dtor(void *p, size_t n)
{
  struct KSemaphore *sem = (struct KSemaphore *) p;
  (void) n;

  assert(!k_list_empty(&sem->queue));
}
