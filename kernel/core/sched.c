
#include <kernel/core/config.h>

#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/core/task.h>
#include <kernel/core/mutex.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

void k_arch_switch(struct Context **, struct Context *);

KLIST_DECLARE(_k_sched_timeouts);

static struct KListLink sched_queue[K_TASK_MAX_PRIORITIES];
struct KSpinLock _k_sched_spinlock = K_SPINLOCK_INITIALIZER("sched");

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel tasks.
 */
void
k_sched_init(void)
{
  int i;

  for (i = 0; i < K_TASK_MAX_PRIORITIES; i++)
    k_list_init(&sched_queue[i]);
}

// Add the specified task to the run queue with the corresponding priority
void
_k_sched_enqueue(struct KTask *th)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("scheduler not locked");

  th->state = K_TASK_STATE_READY;
  k_list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority task from the run queue
static struct KTask *
k_sched_dequeue(void)
{
  struct KListLink *link;
  int i;
  
  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("scheduler not locked");

  for (i = 0; i < K_TASK_MAX_PRIORITIES; i++) {
    if (!k_list_is_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      k_list_remove(link);

      return KLIST_CONTAINER(link, struct KTask, link);
    }
  }

  return NULL;
}

static void
k_sched_switch(struct KTask *task)
{
  struct KCpu *my_cpu = _k_cpu();

  task->cpu = my_cpu;
  my_cpu->task = task;

#ifdef K_ON_TASK_BEFORE_SWITCH
  K_ON_TASK_BEFORE_SWITCH(task);
#endif

  task->state = K_TASK_STATE_RUNNING;

  k_arch_switch(&my_cpu->sched_context, task->context);

#ifdef K_ON_TASK_AFTER_SWITCH
  K_ON_TASK_AFTER_SWITCH(task);
#endif

  my_cpu->task = NULL;
  task->cpu = NULL;
}

static void
k_sched_idle(void)
{
  _k_sched_unlock();

  k_irq_enable();

#ifdef K_ON_TASK_IDLE
  K_ON_TASK_IDLE();
#endif

  arch_task_idle();

  _k_sched_lock();
}

/**
 * Start the scheduler main loop. This function never returns.
 */
void
k_sched_start(void)
{
  _k_sched_lock();

  for (;;) {
    struct KTask *next = k_sched_dequeue();

    if (next != NULL) {
      k_assert(next->state == K_TASK_STATE_READY);
      k_sched_switch(next);
    } else {
      k_sched_idle();
    }
  }
}

// Switch back from the current task context back to the scheduler loop
void
_k_sched_yield_locked(void)
{
  int irq_flags;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("scheduler not locked");

  irq_flags = _k_cpu()->irq_flags;
  k_arch_switch(&k_task_current()->context, _k_cpu()->sched_context);
  _k_cpu()->irq_flags = irq_flags;
}

void
_k_sched_add(struct KListLink *queue, struct KTask *task)
{
  struct KListLink *l;
  struct KTask *other_task;

  for (l = queue->next; l != queue; l = l->next) {
    other_task = KLIST_CONTAINER(l, struct KTask, link);
    if (_k_sched_priority_cmp(task, other_task) > 0)
      break;
  }

  k_list_add_back(l, &task->link);
}

/**
 * Put the current task into sleep.
 *
 * @param queue An optional queue to insert the task into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
int
_k_sched_sleep(struct KListLink *queue, int state, unsigned long timeout,
               struct KSpinLock *lock)
{
  struct KCpu *my_cpu;
  struct KTask *my_task;

  if (lock != NULL) {
    _k_sched_lock();
    k_spinlock_release(lock);
  }

  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("scheduler not locked");

  my_cpu = _k_cpu();
  my_task = my_cpu->task;

  if (my_cpu->lock_count > 0)
    k_panic("called from an IRQ context");
  if (my_cpu->task == NULL)
    k_panic("called not by a task");

  if (timeout != 0) {
    _k_timeout_enqueue(&_k_sched_timeouts, &my_task->timer, timeout);
  }

  my_task->state = state;

  if (queue != NULL)
    _k_sched_add(queue, my_task);

  _k_sched_yield_locked();

  if (timeout != 0) {
    if (my_task->timer.link.next != NULL) {
      _k_timeout_dequeue(&_k_sched_timeouts, &my_task->timer);
    }
  }

  // someone may call this function while holding _k_sched_spinlock?
  if (lock != NULL) {
    _k_sched_unlock();
    k_spinlock_acquire(lock);
  }

  return my_task->sleep_result;
}

void
_k_sched_raise_priority(struct KTask *task, int priority)
{
  k_assert(k_spinlock_holding(&_k_sched_spinlock));
  k_assert(task->priority > priority);

  task->priority = priority;

  // TODO: change priorities for all owned mutexes

  switch (task->state) {
  case K_TASK_STATE_READY:
    // Move into another run queue
    k_list_remove(&task->link);
    _k_sched_enqueue(task);
    break;
  case K_TASK_STATE_MUTEX:
    // Re-insert to update priority
    k_list_remove(&task->link);
    _k_sched_add(&task->sleep_on_mutex->queue, task);
  
    _k_mutex_may_raise_priority(task->sleep_on_mutex, task->priority);
    break;
  default:
    break;
  }
}

void
_k_sched_resume(struct KTask *task, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("sched not locked");

  switch (task->state) {
  case K_TASK_STATE_SLEEP:
    k_list_remove(&task->link);
    break;
  case K_TASK_STATE_MUTEX:
    k_list_remove(&task->link);
  
    // TODO: this may lead to decreasing mutex priority

    break;
  default:
    return;
  }

  k_list_remove(&task->link);

  task->sleep_result = result;

  _k_sched_enqueue(task);
  _k_sched_may_yield(task);
}

// Resume all tasks waiting on the given queue
// The caller must be holding the scheduler lock
void
_k_sched_wakeup_all_locked(struct KListLink *queue, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("sched not locked");

  while (!k_list_is_empty(queue)) {
    struct KListLink *link = queue->next;
    struct KTask *task = KLIST_CONTAINER(link, struct KTask, link);

    _k_sched_resume(task, result);
  }
}

// Resume and return the highest priority task waiting on the given queue
// The caller must be holding the scheduler lock
struct KTask *
_k_sched_wakeup_one_locked(struct KListLink *queue, int result)
{
  struct KTask *task;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("sched not locked");

  if (k_list_is_empty(queue))
    return NULL;

  task = KLIST_CONTAINER(queue->next, struct KTask, link);

  _k_sched_resume(task, result);

  return task;
}

// Check whether a reschedule is required (taking into account the priority
// of a task most recently added to the run queue)
void
_k_sched_may_yield(struct KTask *task)
{
  struct KCpu *my_cpu;
  struct KTask *my_task;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    k_panic("scheduler not locked");

  my_cpu = _k_cpu();
  my_task = my_cpu->task;

  if ((my_task != NULL) && (_k_sched_priority_cmp(task, my_task) > 0)) {
    if (my_cpu->lock_count > 0) {
      // Cannot yield right now, delay until the last call to k_irq_handler_end()
      // or task_unlock().
      my_task->flags |= K_TASK_FLAG_RESCHEDULE;
    } else {
      _k_sched_enqueue(my_task);
      _k_sched_yield_locked();
    }
  }
}

void
k_task_timeout_callback(struct KTimeout *entry)
{
  struct KTask *task = KLIST_CONTAINER(entry, struct KTask, timer);

  switch (task->state) {
  case K_TASK_STATE_MUTEX:
    // TODO
    // fall through
  case K_TASK_STATE_SLEEP:
    _k_sched_resume(task, -ETIMEDOUT);
    break;
  }
}

void
_k_sched_tick(void)
{
  struct KTask *current_task = k_task_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for other sheduling policies
  if (current_task != NULL) {
    _k_sched_lock();
    current_task->flags |= K_TASK_FLAG_RESCHEDULE;
    _k_sched_unlock();
  }

  if (k_cpu_id() == 0) {
    _k_sched_lock();
    _k_timeout_process_queue(&_k_sched_timeouts, k_task_timeout_callback);
    _k_sched_unlock();
  }
}

void
_k_sched_update_effective_priority(void)
{
  struct KTask *task = k_task_current();
  int new_priority, max_mutex_priority;

  new_priority = task->saved_priority;

  max_mutex_priority = _k_mutex_get_highest_priority(&task->owned_mutexes);
  if (max_mutex_priority < new_priority)
    new_priority = max_mutex_priority;

  task->priority = new_priority;
}
