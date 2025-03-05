#include <kernel/core/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/console.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/core/task.h>
#include <kernel/core/spinlock.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/vm.h>
#include <kernel/vm.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

#include "core_private.h"

static void k_task_run(void);

/**
 * Resume execution of a previously suspended task (or begin execution of a
 * newly created one).
 * 
 * @param task The kernel task to resume execution
 */
int
k_task_resume(struct KTask *task)
{
  _k_sched_lock();

  if (task->state != K_TASK_STATE_SUSPENDED) {
    _k_sched_unlock();
    return -EINVAL;
  }

  k_assert(k_list_is_null(&task->link));

  _k_sched_enqueue(task);
  _k_sched_may_yield(task);

  _k_sched_unlock();

  return 0;
}

/**
 * Relinguish the CPU allowing another task to run.
 */
void
k_task_yield(void)
{
  struct KTask *current = k_task_current();
  
  if (current == NULL)
    k_panic("no current task");

  _k_sched_lock();

  _k_sched_enqueue(current);
  _k_sched_yield_locked();

  _k_sched_unlock();
}

// Execution of each task begins here.
static void
k_task_run(void)
{
  struct KTask *my_task = k_task_current();

  // Still holding the scheduler lock (acquired in k_sched_start)
  _k_sched_unlock();

  k_irq_enable();

  my_task->entry(my_task->arg);

  k_task_exit();
}

void
k_task_interrupt(struct KTask *task)
{
  _k_sched_lock();

  if (!task->sleep_on_mutex) {
    // TODO: if task is running, send an SGI
    _k_sched_resume(task, -EINTR);
  }

  _k_sched_unlock();
}

/**
 * Initialize the kernel task. After successful initialization, the task
 * is placed into suspended state and must be explicitly made runnable by a call
 * to k_task_resume().
 * 
 * @param process  Pointer to a process the task belongs to.
 * @param task   Pointer to the kernel task to be initialized.
 * @param entry    task entry point function.
 * @param priority task priority value.
 * @param stack    Top of the task stack.
 * 
 * @return 0 on success.
 */
struct KTask *
k_task_create(void *ext, void (*entry)(void *), void *arg, int priority)
{
  struct Page *stack_page;
  struct KTask *task;
  uint8_t *stack;

  if ((task = (struct KTask *) k_object_pool_get(k_task_cache)) == NULL)
    return NULL;

  if ((stack_page = page_alloc_one(0, PAGE_TAG_KSTACK)) == NULL) {
    k_object_pool_put(k_task_cache, task);
    return NULL;
  }

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  k_list_init(&task->owned_mutexes);
  k_list_null(&task->link);
  task->sleep_on_mutex     = NULL;

  task->flags          = 0;
  task->saved_priority = priority;
  task->priority       = priority;
  task->state          = K_TASK_STATE_SUSPENDED;
  task->entry          = entry;
  task->arg            = arg;
  task->err            = 0;
  task->ext            = ext;
  task->kstack         = stack;

  _k_timeout_init(&task->timer);

  arch_task_init_stack(task, k_task_run);

  return task;
}

/**
 * Destroy the specified task
 */
void
k_task_exit(void)
{
  struct KTask *task = k_task_current();
  extern struct KListLink _k_tasks_to_destroy;

  if (task == NULL)
    k_panic("no current task");

  _k_timeout_fini(&task->timer);

  _k_sched_lock();

  task->state = K_TASK_STATE_DESTROYED;

  k_list_add_back(&_k_tasks_to_destroy, &task->link);

  _k_sched_yield_locked();

  _k_sched_unlock();

  k_panic("should not return");
}

void
k_task_suspend(void)
{
  struct KTask *task = k_task_current();

  _k_sched_lock();
  
  task->state = K_TASK_STATE_SUSPENDED;
  _k_sched_yield_locked();

  _k_sched_unlock();
}

/**
 * Get the currently executing task.
 * 
 * @return A pointer to the currently executing task or NULL
 */
struct KTask *
k_task_current(void)
{
  struct KTask *task;

  k_irq_state_save();
  task = _k_cpu()->task;
  k_irq_state_restore();

  return task;
}
