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
  // TODO: notify pending tasks
  (void) sem;

  return 0;
}

int
ksem_get(struct KSemaphore *sem, unsigned long timeout)
{
  struct Task *my_task = task_current();
  int ret;

  if (my_task == NULL)
    panic("no current task");
  
  sched_lock();

  if ((my_task->lock_count> 0) || (cpu_current()->isr_nesting > 0))
    panic("bad context");

  my_task->u.semaphore.result = 0;

  while ((sem->count == 0) && (my_task->u.semaphore.result == 0)) {
    if (timeout != 0) {
      my_task->timer.remain = timeout;
      ktimer_start(&my_task->timer);
    }

    task_sleep(&sem->queue, TASK_STATE_SEMAPHORE, &__sched_lock);
  }

  if ((ret = my_task->u.semaphore.result) == 0)
    ret = --sem->count; 

  sched_unlock();
  return ret;
}

int
ksem_put(struct KSemaphore *sem)
{
  sched_lock();

  sem->count++;
  sched_wakeup_all(&sem->queue);

  sched_unlock();
  return 0;
}
