
#include <kernel/core/config.h>

#include <kernel/core/assert.h>
#include <kernel/core/irq.h>
#include <kernel/core/task.h>
#include <kernel/core/mutex.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

/**
 * @brief Architecture-specific context switch routine.
 *
 * This function switches CPU context from the current running task to a new task.
 * It must be implemented in architecture-specific code.
 *
 * @param from Location to store pointer to the scheduler context.
 * @param to   Pointer to the next task’s context.
 */
void k_arch_switch(void *, void *);

// Global list of task timeout events.
K_LIST_DECLARE(_k_sched_timeouts);

// Array of ready queues for each task priority.
// Each queue holds tasks ready to run at the corresponding priority level.
static struct KListLink k_sched_queue[K_TASK_MAX_PRIORITIES];

// Global spinlock protecting the scheduler data structures.
struct KSpinLock _k_sched_spinlock = K_SPINLOCK_INITIALIZER("k_sched");

/**
 * @brief Initialize the scheduler subsystem.
 *
 * This function sets up all ready queues to an empty state. It must be called
 * exactly once during system startup, and only by the master CPU in an SMP
 * system.
 */
void
k_sched_init(void)
{
  int i;

  for (i = 0; i < K_TASK_MAX_PRIORITIES; i++)
    k_list_init(&k_sched_queue[i]);
}

// Marks the given task as ready and adds it to the appropriate ready queue
// based on its priority.
// The scheduler spinlock (`_k_sched_spinlock`) must be held.
void
_k_sched_enqueue(struct KTask *task)
{
  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  task->state = K_TASK_STATE_READY;
  k_list_add_back(&k_sched_queue[task->priority], &task->link);
}

static struct KTask *k_sched_dequeue(void);
static void k_sched_idle(void);

/**
 * @brief Start the scheduler main loop on the current CPU.
 *
 * This function never returns. It continuously selects the next ready task
 * and performs context switches. If no task is ready, the CPU enters the idle
 * state until a task becomes runnable.
 *
 * On SMP systems, each CPU must call this function after local initialization
 * to join the scheduler. The master CPU typically calls it after invoking
 * `k_sched_init()`, while other CPUs call it during secondary startup.
 */
void
k_sched_start(void)
{
  struct KCpu *my_cpu;;
  struct KTask *prev_task = NULL;

  _k_sched_lock();

  my_cpu = _k_cpu();

  for (;;) {
    struct KTask *task = k_sched_dequeue();

    if (prev_task != task) {
      #ifdef K_ON_TASK_SWITCH
        K_ON_TASK_SWITCH(prev_task, task);
      #endif
    }

    if (task != K_NULL) {
      k_assert(task->state == K_TASK_STATE_READY);

      task->cpu = my_cpu;
      my_cpu->task = task;

      task->state = K_TASK_STATE_RUNNING;
      k_arch_switch(&my_cpu->sched_context, task->context);

      my_cpu->task = K_NULL;
      task->cpu = K_NULL;
    } else {
      k_sched_idle();
    }

    prev_task = task;
  }
}

// Dequeue the highest-priority ready task.
static struct KTask *
k_sched_dequeue(void)
{
  int i;

  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  for (i = 0; i < K_TASK_MAX_PRIORITIES; i++) {
    if (!k_list_is_empty(&k_sched_queue[i])) {
      struct KListLink *link = k_sched_queue[i].next;
      k_list_remove(link);

      return K_CONTAINER_OF(link, struct KTask, link);
    }
  }

  return K_NULL;
}



// Enter the idle state when no task is ready.
static void
k_sched_idle(void)
{
  _k_sched_unlock();

  k_irq_enable();

#ifdef K_ON_SCHED_IDLE
  K_ON_SCHED_IDLE();
#endif

  k_arch_task_idle();

  _k_sched_lock();
}

// Yield the CPU to another ready task while holding the scheduler lock.
// The interrupt flags are preserved across the context switch to maintain
// consistent CPU interrupt state.
void
_k_sched_yield_locked(struct KTask *current)
{
  int irq_flags;

  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  irq_flags = current->cpu->irq_flags;
  k_arch_switch(&current->context, current->cpu->sched_context);
  current->cpu->irq_flags = irq_flags;
}

static void _k_sched_sleep_queue_add(struct KListLink *, struct KTask *);

// This function blocks the current task, optionally adding it to a
// synchronization or wait queue, and switches context to allow another
// task to execute.
// If a timeout is provided, the task is also added to the global timeout
// queue and will automatically wake after the timeout expires unless woken
// earlier.
// Optional spinlock protecting the resource being waited on can be provided.
// It will be released before sleeping and reacquired after wake-up.
int
_k_sched_sleep(struct KListLink *queue, int state, k_tick_t timeout,
               struct KSpinLock *lock)
{
  struct KCpu *my_cpu;
  struct KTask *my_task;

  if (lock != K_NULL) {
    _k_sched_lock();
    k_spinlock_release(lock);
  }

  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  my_cpu = _k_cpu();
  my_task = my_cpu->task;

  k_assert(my_cpu->lock_count == 0);
  k_assert(my_cpu->task != K_NULL);

  if (timeout != 0)
    _k_timeout_queue_add(&_k_sched_timeouts, &my_task->timer, timeout);

  my_task->state = state;

  if (queue != K_NULL)
    _k_sched_sleep_queue_add(queue, my_task);

  _k_sched_yield_locked(my_task);

  if ((timeout != 0) && (my_task->timer.link.next != K_NULL))
    _k_timeout_queue_remove(&_k_sched_timeouts, &my_task->timer);

  if (lock != K_NULL) {
    _k_sched_unlock();
    k_spinlock_acquire(lock);
  }

  return my_task->sleep_result;
}

// Add a task to a sleep or wait queue.
// Inserts the given task into a wait queue in priority order.
// Higher-priority tasks are placed ahead of lower-priority ones,
// ensuring that wake-ups occur in priority order.
static void
_k_sched_sleep_queue_add(struct KListLink *queue, struct KTask *task)
{
  struct KListLink *l;

  for (l = queue->next; l != queue; l = l->next) {
    struct KTask *other_task = K_CONTAINER_OF(l, struct KTask, link);
    if (_k_sched_priority_cmp(task, other_task) > 0)
      break;
  }

  k_list_add_back(l, &task->link);
}

void
_k_sched_wakeup_all_locked(struct KListLink *queue, int result)
{
  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  while (!k_list_is_empty(queue)) {
    struct KListLink *link = queue->next;
    struct KTask *task = K_CONTAINER_OF(link, struct KTask, link);

    _k_sched_resume(task, result);
  }
}

struct KTask *
_k_sched_wakeup_one_locked(struct KListLink *queue, int result)
{
  struct KTask *task;

  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  if (k_list_is_empty(queue))
    return K_NULL;

  task = K_CONTAINER_OF(queue->next, struct KTask, link);

  _k_sched_resume(task, result);

  return task;
}

void
_k_sched_resume(struct KTask *task, int result)
{
  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  if ((task->state != K_TASK_STATE_SLEEP) &&
      (task->state != K_TASK_STATE_SLEEP_UNWAKEABLE))
    return;

  // FIXME: mutex priority
  
  k_list_remove(&task->link);
  task->sleep_result = result;
  _k_sched_enqueue(task);
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
  case K_TASK_STATE_SLEEP:
  case K_TASK_STATE_SLEEP_UNWAKEABLE:
    if (task->sleep_on_mutex != K_NULL) {
      // Re-insert to update priority
      k_list_remove(&task->link);
      _k_sched_sleep_queue_add(&task->sleep_on_mutex->queue, task);
      _k_mutex_may_raise_priority(task->sleep_on_mutex, task->priority);
    }
    
    break;
  default:
    break;
  }
}

void
_k_sched_may_yield(struct KTask *task)
{
  struct KCpu *my_cpu;
  struct KTask *my_task;

  k_assert(k_spinlock_holding(&_k_sched_spinlock));

  my_cpu = _k_cpu();
  my_task = my_cpu->task;

  if ((my_task != K_NULL) && (_k_sched_priority_cmp(task, my_task) > 0)) {
    if (my_cpu->lock_count > 0) {
      // Cannot yield right now, delay until the last call to k_irq_handler_end()
      // or task_unlock().
      my_task->flags |= K_TASK_FLAG_RESCHEDULE;
    } else {
      _k_sched_enqueue(my_task);
      _k_sched_yield_locked(my_task);
    }
  }
}

void
k_task_timeout_callback(struct KTimeoutEntry *entry)
{
  struct KTask *task = K_CONTAINER_OF(entry, struct KTask, timer);

  switch (task->state) {
  case K_TASK_STATE_SLEEP:
  case K_TASK_STATE_SLEEP_UNWAKEABLE:
    _k_sched_resume(task, -ETIMEDOUT);
    break;
  }
}

void
_k_sched_check_quantum(void)
{
  struct KTask *current_task = k_task_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for other sheduling policies
  if (current_task != K_NULL) {
    _k_sched_lock();
    current_task->flags |= K_TASK_FLAG_RESCHEDULE;
    _k_sched_unlock();
  }
}

void
_k_sched_adjust_timeouts(k_tick_t delta_tick)
{
  _k_sched_lock();
  _k_timeout_queue_adjust(&_k_sched_timeouts, k_task_timeout_callback, delta_tick);
  _k_sched_unlock();
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
