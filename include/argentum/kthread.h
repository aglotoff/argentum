#ifndef __INCLUDE_ARGENTUM_KTHREAD_H__
#define __INCLUDE_ARGENTUM_KTHREAD_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>

#include <argentum/list.h>
#include <argentum/spinlock.h>

struct Process;

#define KTHREAD_MAX_PRIORITIES  (2 * NZERO)

enum {
  KTHREAD_RUNNABLE     = 1,
  KTHREAD_RUNNING      = 2,
  KTHREAD_NOT_RUNNABLE = 3,
  KTHREAD_SUSPENDED    = 4,
  KTHREAD_DESTROYED    = 5,
};

enum {
  KTHREAD_RESCHEDULE = (1 << 0),
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

/**
 * thread state.
 */
struct KThread {
  struct ListLink   link;           ///< Link into the containing list
  int               state;          ///< Thread state
  int               priority;       ///< Thread priority
  uint8_t          *kstack;         ///< Bottom of the kernel-mode stack
  struct Context   *context;        ///< Saved context
  void            (*entry)(void);   ///< Thread entry point
  struct Process   *process;        ///< The process this thread belongs to
  int               flags;
};

struct KThread *kthread_current(void);
void            kthread_enqueue(struct KThread *);
struct KThread *kthread_create(struct Process *, void (*)(void), int, uint8_t *);
void            kthread_destroy(struct KThread *);
int             kthread_resume(struct KThread *);
void            kthread_run(void);
void            kthread_yield(void);
void            kthread_sleep(struct ListLink *, int);
void            kthread_wakeup_all(struct ListLink *);

extern struct SpinLock __sched_lock;

void            sched_init(void);
void            sched_start(void);
void            sched_yield(void);

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

static inline int
sched_locked(void)
{
  return spin_holding(&__sched_lock);
}

#endif  // __INCLUDE_ARGENTUM_KTHREAD_H__
