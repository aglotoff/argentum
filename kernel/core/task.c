#include <kernel/core/config.h>

#include <kernel/core/assert.h>
#include <kernel/core/irq.h>
#include <kernel/core/task.h>
#include <kernel/core/spinlock.h>

#include "core_private.h"

// Used by the kernel to verify that the object is a valid task
#define K_TASK_TAG     0x5441534B  // {'T','A','S','K'}

static void k_task_run(void);

/**
 * @brief Initialize a task control block and prepare it for execution.
 *
 * Sets up the task’s initial context, stack, priority, and state, and
 * configures its timeout object and bookkeeping fields. The task is created
 * in the suspended state; it will not run until `k_task_start()` is called.
 *
 * @param task        Pointer to a `KTask` structure to initialize.
 * @param ext         User-defined extension pointer stored in the task
 *                    control block.
 * @param entry       Function that will serve as the task’s entry point.
 * @param arg         Argument passed to the entry function upon startup.
 * @param stack       Pointer to the stack region allocated for the task.
 * @param stack_size  Size of the provided stack region in bytes.
 * @param priority    Initial scheduling priority for the task.
 *
 * @return 0 on success; always succeeds unless invalid parameters are passed,
 *         in which case assertions may trigger in debug builds.
 *
 * @note This function does not enqueue the task for execution.
 *       Call `k_task_start()` to make the task runnable.
 */
int
k_task_create(struct KTask *task,
              void *ext,
              void (*entry)(void *), void *arg,
              void *stack, k_size_t stack_size,
              int priority)
{
  k_list_init(&task->owned_mutexes);
  k_list_null(&task->link);
  task->object_tag = K_TASK_TAG;
  task->sleep_on_mutex = K_NULL;
  task->flags = 0;
  task->saved_priority = priority;
  task->priority = priority;
  task->state = K_TASK_STATE_INIT;
  task->entry = entry;
  task->arg = arg;
  task->ext = ext;
  task->kstack = stack;
  task->kstack_size = stack_size;
  task->err = 0;

  _k_timeout_entry_create(&task->timer);

  k_arch_task_init_stack(task, k_task_run);

  return 0;
}

/**
 * @brief Transition a newly created task into the ready state.
 *
 * Schedules a task for execution for the first time. The task must be in
 * the `K_TASK_STATE_INIT` state, indicating that it has been created but
 * has never run. If the state is invalid, the operation fails.
 *
 * Upon success, the task is enqueued in the scheduler’s ready queue and
 * may begin execution immediately depending on system state and priority.
 *
 * @param task Pointer to a valid, initialized task control block.
 *
 * @return 0 on success; `K_ERR_INVAL` if the task is not in the
 *         initialization state.
 *
 * @note The task must have been fully initialized by `k_task_create()` and
 *       must not have been started previously.
 */
int
k_task_start(struct KTask *task)
{
  k_assert(task != K_NULL);
  k_assert(task->object_tag == K_TASK_TAG);

  _k_sched_lock();

  if (task->state != K_TASK_STATE_INIT) {
    _k_sched_unlock();
    return K_ERR_INVAL;
  }

  k_assert(k_list_is_null(&task->link));

  _k_sched_enqueue(task);
  _k_sched_may_yield(task);

  _k_sched_unlock();

  return 0;
}

/**
 * @brief Voluntarily yield the processor to another runnable task.
 *
 * The calling task is reinserted into the ready queue, allowing the
 * scheduler to select a different task to run. This is a cooperative
 * scheduling hint and does not change the calling task’s state.
 */
void
k_task_yield(void)
{
  struct KTask *current = k_task_current();
  
  k_assert(current != K_NULL);
  k_assert(current->object_tag == K_TASK_TAG);

  _k_sched_lock();

  _k_sched_enqueue(current);
  _k_sched_yield_locked();

  _k_sched_unlock();
}

// Execution of each task begins here.
static void
k_task_run(void)
{
  struct KTask *current = k_task_current();

  k_assert(current != K_NULL);
  k_assert(current->object_tag == K_TASK_TAG);

  // Release the lock acquired in k_sched_start
  _k_sched_unlock();

  k_irq_enable();

  current->entry(current->arg);

  k_task_exit();
}

/**
 * @brief Wake a sleeping task.
 *
 * If the task is sleeping on a condition other than an unwakeable sleep
 * state, it is transitioned back to the ready state and given an error code
 * of `K_ERR_INTR` to indicate that its wait was interrupted.
 *
 * @param task Pointer to the task control block to wake.
 *
 * @note Tasks in the `K_TASK_STATE_SLEEP_UNWAKEABLE` state are not awakened.
 */
void
k_task_wake(struct KTask *task)
{
  k_assert(task != K_NULL);
  k_assert(task->object_tag == K_TASK_TAG);

  _k_sched_lock();

  if (task->state != K_TASK_STATE_SLEEP_UNWAKEABLE) {
    // TODO: if the task is running, send an SGI
    _k_sched_resume(task, K_ERR_INTR);
  }

  _k_sched_unlock();
}

/**
 * @brief Terminate the calling task and remove it from scheduling.
 *
 * Marks the task as destroyed and yields permanently. The task never returns
 * from this function.
 *
 * @note This function must be invoked only by the task that intends to exit.
 *       Task memory and stack are not freed here; upper layers are responsible
 *       for cleanup if supported by the system design.
 */
void
k_task_exit(void)
{
  struct KTask *current = k_task_current();

  k_assert(current != K_NULL);
  k_assert(current->object_tag == K_TASK_TAG);

  _k_timeout_entry_destroy(&current->timer);

  _k_sched_lock();

  current->state = K_TASK_STATE_DESTROYED;

#ifdef K_ON_TASK_DESTROY
  K_ON_TASK_DESTROY(current);
#endif

  _k_sched_yield_locked();

  current->object_tag = 0;

  _k_sched_unlock();

  k_panic("should not return");
}

/**
 * @brief Put the calling task to sleep for a specified timeout period.
 *
 * Transitions the current task into the sleeping state and yields control to
 * the scheduler. The task remains asleep until either:
 *   - the specified timeout expires, or
 *   - it is explicitly woken by `k_task_wake()` or another wake mechanism.
 *
 * The return value indicates the reason the task resumed execution.
 *
 * @param timeout Number of ticks to sleep. A value of zero causes an
 *                immediate yield without blocking. A value of 0 suspends the
 *                task indefinitely.
 *
 * @return 0 on normal timeout expiration; or an error code such as
 *         `K_ERR_INTR` if the sleep was interrupted.
 */
int
k_task_sleep(k_tick_t timeout)
{
  struct KTask *current = k_task_current();
  int r;

  k_assert(current != K_NULL);
  k_assert(current->object_tag == K_TASK_TAG);

  _k_sched_lock();

  r = _k_sched_sleep(K_NULL,
                     K_TASK_STATE_SLEEP,
                     timeout,
                     K_NULL);

  _k_sched_unlock();

  return r;
}

/**
 * @brief Retrieve the task control block of the currently executing task.
 *
 * @return Pointer to the current `KTask` structure, or NULL if no task is
 *         running (e.g., before scheduler start-up).
 */
struct KTask *
k_task_current(void)
{
  struct KTask *current;

  k_irq_state_save();
  current = _k_cpu()->task;
  k_irq_state_restore();

  return current;
}
