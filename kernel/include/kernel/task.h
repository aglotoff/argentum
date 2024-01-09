#ifndef __KERNEL_INCLUDE_KERNEL_TASK_H__
#define __KERNEL_INCLUDE_KERNEL_TASK_H__

#ifndef __OSDEV_KERNEL__
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
  TASK_STATE_SLEEPING,
  TASK_STATE_SUSPENDED,
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

struct KMemCache;
extern struct KMemCache *thread_cache;

struct Process;

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
  void            (*entry)(void *);
  void             *arg;
  /** Various flags */
  int               flags;
  /** Task waiting for this task to exit */
  struct Task      *destroyer;
  /** Timer for timeouts */
  struct KTimer     sleep_timer;
  /** Value that indicated sleep result */
  int               sleep_result;
  int               err;
    /** Tne process this thread belongs to */
  struct Process   *process;

  /** Bottom of the kernel-mode thread stack */
  uint8_t              *kstack;
  /** Address of the current trap frame on the stack */
  struct TrapFrame     *tf;

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
int          task_create(struct Task *, struct Process *, void (*)(void *), void *, int, uint8_t *);
void         task_exit(void);
int          task_resume(struct Task *);
void         task_yield(void);
int          sched_sleep(struct ListLink *, unsigned long, struct SpinLock *);
void         task_cleanup(struct Task *);

void         sched_wakeup_all(struct ListLink *, int);
void         sched_wakeup_one(struct ListLink *, int);
void         sched_init(void);
void         sched_start(void);
void         ktime_tick(void);
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
