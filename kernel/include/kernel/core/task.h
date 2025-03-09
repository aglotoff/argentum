#ifndef __KERNEL_INCLUDE_KERNEL_TASK_H__
#define __KERNEL_INCLUDE_KERNEL_TASK_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/core/list.h>
#include <kernel/core/tick.h>
#include <arch/context.h>

enum {
  K_TASK_STATE_NONE = 0,
  K_TASK_STATE_READY,
  K_TASK_STATE_RUNNING,
  K_TASK_STATE_SLEEP,
  K_TASK_STATE_MUTEX,
  K_TASK_STATE_SUSPENDED,
  K_TASK_STATE_DESTROYED,
};

enum {
  K_TASK_FLAG_RESCHEDULE = (1 << 0),
  K_TASK_FLAG_DESTROY    = (1 << 1),
};

struct KObjectPool;
extern struct KObjectPool *k_task_cache;

struct KMutex;

/**
 * Scheduler task state.
 */
struct KTask {
  char              type[4];
  /** Link into the list containing this task */
  struct KListLink  link;
  /** Current task state */
  int               state;
  /** Task priority value */
  int               priority;
  int               saved_priority;
  /** Various flags */
  int               flags;
  /** CPU */
  struct KCpu       *cpu;

  struct KListLink   owned_mutexes;
  struct KMutex     *sleep_on_mutex;

  /** Bottom of the kernel-mode stack */
  void              *kstack;
  /** Saved kernel context */
  struct Context    *context;

  /** Entry point function */
  void            (*entry)(void *);
  /** The argument for the entry function*/
  void             *arg;

  /** Timer for timeouts */
  struct KTimeout timer;
  /** Value that indicated sleep result */
  int               sleep_result;
  int               err;

  /** Tne process this task belongs to */
  void             *ext;
};

void          arch_task_init_stack(struct KTask *, void (*)(void));
void          arch_task_idle(void);

struct KTask *k_task_current(void);
int           k_task_create(struct KTask *, void *, void (*)(void *), void *, void *, int);
void          k_task_exit(void);
int           k_task_resume(struct KTask *);
void          k_task_suspend(void);
void          k_task_yield(void);
void          k_task_interrupt(struct KTask *);

void          k_sched_init(void);
void          k_sched_start(void);

#endif  // __KERNEL_INCLUDE_KERNEL_TASK_H__
