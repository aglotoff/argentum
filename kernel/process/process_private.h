#ifndef __PROCESS_PRIVATE_H
#define __PROCESS_PRIVATE_H

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

struct Process;

extern struct KSpinLock __process_lock;
extern struct KListLink __process_list;
extern struct KSpinLock __process_sleep_lock;

void _process_continue(struct Process *);
void _process_stop(struct Process *, int);

void _signal_state_change_to_parent(struct Process *);

static inline void
process_lock(void)
{
  k_spinlock_acquire(&__process_lock);
}

static inline void
process_unlock(void)
{
  k_spinlock_release(&__process_lock);
}

static inline void
process_sleep_lock(void)
{
  k_spinlock_acquire(&__process_sleep_lock);
}

static inline void
process_sleep_unlock(void)
{
  k_spinlock_release(&__process_sleep_lock);
}

#endif  // !__PROCESS_PRIVATE_H
