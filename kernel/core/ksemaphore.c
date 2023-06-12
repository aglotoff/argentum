#include <assert.h>
#include <errno.h>

#include <kernel/cpu.h>
#include <kernel/ksemaphore.h>
#include <kernel/task.h>

int
ksem_create(struct KSemaphore *sem, unsigned long initial_count)
{
  list_init(&sem->queue);
  sem->count = initial_count;

  return 0;
}

int
ksem_destroy(struct KSemaphore *sem)
{
  sched_lock();
  sched_wakeup_all(&sem->queue, -EINVAL);
  sched_unlock();

  return 0;
}

int
ksem_get(struct KSemaphore *sem, unsigned long timeout, int blocking)
{
  struct Task *my_task = task_current();
  int ret;

  if ((my_task == NULL) && blocking)
    // TODO: choose another value to indicate an error?
    return -EAGAIN;

  sched_lock();

  while (sem->count == 0) {
    struct Cpu *my_cpu = cpu_current();

    if (!blocking || (my_task->lock_count > 0) || (my_cpu->isr_nesting > 0)) {
      // Can't block
      sched_unlock();
      return -EAGAIN;
    }

    sched_sleep(&sem->queue, TASK_STATE_SEMAPHORE, timeout, NULL);

    if ((ret = my_task->sleep_result) != 0) {
      sched_unlock();
      return ret;
    }
  }

  ret = --sem->count;

  sched_unlock();
  return ret;
}

int
ksem_put(struct KSemaphore *sem)
{
  sched_lock();

  sem->count++;
  sched_wakeup_one(&sem->queue, 0);

  sched_unlock();
  return 0;
}
