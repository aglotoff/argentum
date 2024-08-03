#ifndef __CORE_PRIVATE_H
#define __CORE_PRIVATE_H

#include <kernel/spinlock.h>
#include <kernel/thread.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>

struct Context;
struct KListLink;
struct KMutex;
struct KTimer;

void            _k_sched_resume(struct KThread *, int);
void            _k_sched_may_yield(struct KThread *);
void            _k_sched_yield_locked(void);
void            _k_sched_enqueue(struct KThread *);
void            _k_sched_wakeup_all_locked(struct KListLink *, int);
struct KThread *_k_sched_wakeup_one_locked(struct KListLink *, int);
int             _k_sched_sleep(struct KListLink *, int, unsigned long, struct KSpinLock *);
void            _k_sched_raise_priority(struct KThread *, int);
void            _k_sched_recalc_priority(struct KThread *);

void            _k_mutex_may_raise_priority(struct KMutex *, int);

void            _k_timer_start(struct KTimer *, unsigned long);

extern struct KSpinLock _k_sched_spinlock;

// Compare thread priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static inline int
_k_sched_priority_cmp(struct KThread *t1, struct KThread *t2)
{
  return t2->priority - t1->priority; 
}

static inline void
_k_sched_lock(void)
{
  k_spinlock_acquire(&_k_sched_spinlock);
  // cprintf("+ %d\n", k_cpu_id());
}

static inline void
_k_sched_unlock(void)
{
  // cprintf("- %d\n", k_cpu_id());
  k_spinlock_release(&_k_sched_spinlock);
}

static inline void
_k_sched_wakeup_all(struct KListLink *thread_list, int result)
{
  _k_sched_lock();
  _k_sched_wakeup_all_locked(thread_list, result);
  _k_sched_unlock();
}

static inline void
_k_sched_wakeup_one(struct KListLink *queue, int result)
{
  _k_sched_lock();
  _k_sched_wakeup_one_locked(queue, result);
  _k_sched_unlock();
}

/**
 * The kernel maintains a special structure for each processor, which
 * records the per-CPU information.
 */
struct KCpu {
  struct Context *sched_context;  ///< Saved scheduler context
  struct KThread *thread;         ///< The currently running kernel task
  int             lock_count;     ///< Sheculer lock nesting level
  int             irq_save_count; ///< Nesting level of k_irq_save() calls
  int             irq_flags;      ///< IRQ state before the first k_irq_save()
};

struct KCpu    *_k_cpu(void);

#endif  // !__CORE_PRIVATE_H
