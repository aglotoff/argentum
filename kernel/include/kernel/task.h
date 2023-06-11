#ifndef __KERNEL_INCLUDE_KERNEL_TASK_H__
#define __KERNEL_INCLUDE_KERNEL_TASK_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>

#include <kernel/ktimer.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>

#define TASK_MAX_PRIORITIES  (2 * NZERO)

enum {
  TASK_STATE_NONE = 0,
  TASK_STATE_READY,
  TASK_STATE_RUNNING,
  TASK_STATE_MUTEX,
  TASK_STATE_SLEEPING_WCHAN,
  TASK_STATE_SLEEPING,      
  TASK_STATE_SUSPENDED,
  TASK_STATE_DESTROY,
  TASK_STATE_DESTROYED,
};

enum {
  TASK_FLAGS_RESCHEDULE = (1 << 0),
  TASK_FLAGS_DESTROY    = (1 << 1),
};

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
  uint32_t s[32];
  uint32_t fpscr;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
};

struct Task;

struct TaskHooks {
  void (*prepare_switch)(struct Task *);
  void (*finish_switch)(struct Task *);
  void (*destroy)(struct Task *);
};

/**
 * Scheduler task state.
 */
struct Task {
  /** Link into the list containing this task */
  struct ListLink   link;
  /** Current task state */
  int               state;
  /** Task priority value */
  int               priority;
  /** Saved kernel context */
  struct Context   *context;
  /** Entry point */
  void            (*entry)(void);
  /** Various flags */
  int               flags;
  /** Hooks to be called for this task */
  struct TaskHooks *hooks;
  /** Task waiting for this task to exit */
  struct Task      *destroyer;
  /** Timer for timeouts */
  struct KTimer     timer;
  /** Count to keep track of nested task_protect() calls */
  int               protect_count;
  /** Count to keep track of nested task_lock() calls */
  int               lock_count;

  /** State-specific information */
  union {
    struct {
      struct Cpu *cpu;
    } running;
    struct {
      struct Task *task;
    } destroy;
  } u;
};

struct Task *task_current(void);
int          task_create(struct Task *, void (*)(void), int, uint8_t *,
                       struct TaskHooks *);
void         task_destroy(struct Task *);
int          task_resume(struct Task *);
void         task_yield(void);
void         task_sleep(struct ListLink *, int, struct SpinLock *lock);
void         task_wakeup_one(struct ListLink *);
void         task_wakeup_all(struct ListLink *);
void         task_lock(struct Task *);
void         task_unlock(struct Task *);
void         task_protect(struct Task *);
int          task_unprotect(struct Task *);
void         task_cleanup(struct Task *);
void         task_delay(unsigned long);

void         sched_wakeup_all(struct ListLink *);
void         sched_init(void);
void         sched_start(void);
void         sched_tick(void);
void         sched_isr_enter(void);
void         sched_isr_exit(void);

extern struct SpinLock __sched_lock;

static inline void
sched_lock(void)
{
  spin_lock(&__sched_lock);
}

static inline void
sched_unlock(void)
{
  spin_unlock(&__sched_lock);
}

#endif  // __KERNEL_INCLUDE_KERNEL_TASK_H__
