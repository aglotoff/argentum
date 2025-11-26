#ifndef __INCLUDE_KERNEL_CORE_TASK_H__
#define __INCLUDE_KERNEL_CORE_TASK_H__

#include <kernel/core/list.h>
#include <kernel/core/tick.h>

enum {
  K_TASK_STATE_NONE = 0,
  K_TASK_STATE_INIT,
  K_TASK_STATE_READY,
  K_TASK_STATE_RUNNING,
  K_TASK_STATE_SLEEP,
  K_TASK_STATE_SLEEP_UNWAKEABLE,
  K_TASK_STATE_DESTROYED,
};

enum {
  K_TASK_FLAG_RESCHEDULE = (1 << 0),
  K_TASK_FLAG_DESTROY    = (1 << 1),
};

struct KMutex;

/**
 * @brief Kernel task control block (TCB).
 *
 * Represents the complete runtime state of a task, including scheduling
 * attributes, execution context, synchronization information, and user-
 * defined extension data.
 */
struct KTask {
  int type;

  struct KListLink link;
  int state;

  int priority;
  int saved_priority;
  int flags;
  struct KCpu *cpu;

  struct KListLink owned_mutexes;
  struct KMutex *sleep_on_mutex;

  void *kstack;
  k_size_t kstack_size;
  void *context;

  void (*entry)(void *);
  void *arg;

  struct KTimeoutEntry timer;
  int sleep_result;
  int err;

  void *ext;
};

/* -------------------------------------------------------------------------- */
/*                           Architecture Interface                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize the architecture-specific stack frame for a new task.
 *
 * Prepares the task’s kernel stack and initial context so that the scheduler
 * can dispatch it. The provided entry function is installed as the initial PC
 * that the task will begin executing when first scheduled.
 *
 * @param task   Pointer to the task control block being initialized.
 * @param entry  Task entry point that eventually invokes the task’s
 *               user-defined entry function.
 */
void k_arch_task_init_stack(struct KTask *, void (*)(void));

/**
 * @brief Architecture-defined idle loop function.
 *
 * Executes when no runnable tasks are available. Implementations typically
 * place the CPU into a low-power state, wait for interrupts, or perform
 * housekeeping activities. This function must never return.
 *
 * @note Called only by the scheduler. Must not manipulate scheduler state.
 */
void k_arch_task_idle(void);

/* -------------------------------------------------------------------------- */
/*                                 Kernel API                                 */
/* -------------------------------------------------------------------------- */

struct KTask *k_task_current(void);
int k_task_create(struct KTask *, void *, void (*)(void *), void *, void *, k_size_t, int);
void k_task_exit(void);
int k_task_start(struct KTask *);
int k_task_sleep(k_tick_t);
void k_task_yield(void);
void k_task_wake(struct KTask *);

void k_sched_init(void);
void k_sched_start(void);

#endif  // __INCLUDE_KERNEL_CORE_TASK_H__
