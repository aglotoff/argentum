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
  KTHREAD_READY          = 1,
  KTHREAD_RUNNING        = 2,
  KTHREAD_SLEEPING_MUTEX = 3,
  KTHREAD_SLEEPING_WCHAN = 4,
  KTHREAD_SUSPENDED      = 5,
  KTHREAD_DESTROYED      = 6,
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
  struct Context   *context;        ///< Saved context
  void            (*entry)(void);   ///< Thread entry point
  struct Process   *process;        ///< The process this thread belongs to
  int               flags;
};

struct KThread *kthread_current(void);
int             kthread_init(struct Process *, struct KThread *, void (*)(void),
                             int, uint8_t *);
void            kthread_destroy(struct KThread *);
int             kthread_resume(struct KThread *);
void            kthread_yield(void);
void            kthread_sleep(struct ListLink *, int, struct SpinLock *lock);
void            kthread_wakeup_one(struct ListLink *);
void            kthread_wakeup_all(struct ListLink *);

void            sched_init(void);
void            sched_start(void);
void            sched_tick(void);
void            sched_isr_enter(void);
void            sched_isr_exit(void);

#endif  // __INCLUDE_ARGENTUM_KTHREAD_H__
