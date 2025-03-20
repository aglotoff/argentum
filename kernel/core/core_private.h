#ifndef __CORE_PRIVATE_H
#define __CORE_PRIVATE_H

#include <kernel/core/spinlock.h>
#include <kernel/core/task.h>
#include <kernel/console.h>
#include <kernel/core/cpu.h>

struct Context;
struct KListLink;
struct KMutex;
struct KTimer;

void            _k_sched_resume(struct KTask *, int);
void            _k_sched_may_yield(struct KTask *);
void            _k_sched_yield_locked(void);
void            _k_sched_enqueue(struct KTask *);
void            _k_sched_wakeup_all_locked(struct KListLink *, int);
struct KTask *_k_sched_wakeup_one_locked(struct KListLink *, int);
int             _k_sched_sleep(struct KListLink *, int, unsigned long, struct KSpinLock *);
void            _k_sched_raise_priority(struct KTask *, int);
void            _k_sched_recalc_priority(struct KTask *);
void            _k_sched_timer_tick(void);
void            _k_sched_update_effective_priority(void);

int             _k_mutex_timed_lock(struct KMutex *, unsigned long);
void            _k_mutex_unlock(struct KMutex *);
int             _k_mutex_get_highest_priority(struct KListLink *);
void            _k_mutex_may_raise_priority(struct KMutex *, int);

void            _k_timer_start(struct KTimer *, unsigned long);
void            _k_timer_tick(void);

void            _k_timeout_process_queue(struct KListLink *, void (*)(struct KTimeout *));
void            _k_timeout_init(struct KTimeout *);
void            _k_timeout_enqueue(struct KListLink *queue, struct KTimeout *entry, unsigned long delay);
void            _k_timeout_dequeue(struct KListLink *queue, struct KTimeout *entry);
void            _k_timeout_fini(struct KTimeout *timer);

extern struct KSpinLock _k_sched_spinlock;

// Compare task priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static inline int
_k_sched_priority_cmp(struct KTask *t1, struct KTask *t2)
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
_k_sched_wakeup_all(struct KListLink *task_list, int result)
{
  _k_sched_lock();
  _k_sched_wakeup_all_locked(task_list, result);
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
  struct KTask   *task;         ///< The currently running kernel task
  int             lock_count;     ///< Sheculer lock nesting level
  int             irq_save_count; ///< Nesting level of k_irq_state_save() calls
  int             irq_flags;      ///< IRQ state before the first k_irq_state_save()
};

struct KCpu    *_k_cpu(void);

#endif  // !__CORE_PRIVATE_H

