#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/thread.h>
#include <kernel/spin.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/vm.h>
#include <kernel/vm.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

#include "core_private.h"

void arch_context_switch(struct Context **, struct Context *);

LIST_DECLARE(threads_to_destroy);
static struct ListLink sched_queue[THREAD_MAX_PRIORITIES];
struct KSpinLock _k_sched_spinlock = K_SPINLOCK_INITIALIZER("sched");

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel threads.
 */
void
k_sched_init(void)
{
  int i;

  thread_cache = k_object_pool_create("thread_cache", sizeof(struct KThread), 0,
                                   NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread cache");

  for (i = 0; i < THREAD_MAX_PRIORITIES; i++)
    list_init(&sched_queue[i]);
}

// Add the specified thread to the run queue with the corresponding priority
void
_k_sched_enqueue(struct KThread *th)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  th->state = THREAD_STATE_READY;
  list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority thread from the run queue
static struct KThread *
k_sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  for (i = 0; i < THREAD_MAX_PRIORITIES; i++) {
    if (!list_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct KThread, link);
    }
  }

  return NULL;
}

static void
k_sched_switch(struct KThread *thread)
{
  struct KCpu *my_cpu = k_cpu();

  if (thread->process != NULL)
    vm_arch_load(thread->process->vm->pgtab);

  thread->state = THREAD_STATE_RUNNING;

  thread->cpu = my_cpu;
  my_cpu->thread = thread;

  arch_context_switch(&my_cpu->sched_context, thread->context);

  my_cpu->thread = NULL;
  thread->cpu = NULL;

  if (thread->process != NULL)
    vm_arch_load_kernel();
}

static void
k_sched_idle(void)
{
  // Cleanup destroyed threads
  while (!list_empty(&threads_to_destroy)) {
    struct KThread *thread;
    struct Page *kstack_page;
    
    thread = LIST_CONTAINER(threads_to_destroy.next, struct KThread, link);
    list_remove(&thread->link);

    _k_sched_unlock();

    // Free the thread kernel stack
    kstack_page = kva2page(thread->kstack);
    kstack_page->ref_count--;
    page_free_one(kstack_page);

    // Free the thread object
    k_object_pool_put(thread_cache, thread);

    _k_sched_lock();
  }

  _k_sched_unlock();

  k_irq_enable();
  asm volatile("wfi");

  _k_sched_lock();
}

/**
 * Start the scheduler main loop. This function never returns.
 */
void
k_sched_start(void)
{
  _k_sched_lock();

  for (;;) {
    struct KThread *next = k_sched_dequeue();

    if (next != NULL) {
      assert(next->state == THREAD_STATE_READY);
      k_sched_switch(next);
    } else {
      k_sched_idle();
    }
  }
}

// Switch back from the current thread context back to the scheduler loop
void
_k_sched_yield(void)
{
  int irq_flags;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  irq_flags = k_cpu()->irq_flags;
  arch_context_switch(&k_thread_current()->context, k_cpu()->sched_context);
  k_cpu()->irq_flags = irq_flags;
}

/**
 * Notify the kernel that an ISR processing has started.
 */
void
sched_isr_enter(void)
{
  k_cpu()->isr_nesting++;
}

/**
 * Notify the kernel that an ISR processing is finished.
 */
void
sched_isr_exit(void)
{
  struct KCpu *my_cpu;

  _k_sched_lock();

  my_cpu = k_cpu();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct KThread *my_thread = my_cpu->thread;

    // Before resuming the current thread, check whether it must give up the CPU
    // or exit.
    if ((my_thread != NULL) && (my_thread->flags & THREAD_FLAG_RESCHEDULE)) {
      my_thread->flags &= ~THREAD_FLAG_RESCHEDULE;

      _k_sched_enqueue(my_thread);
      _k_sched_yield();
    }
  }

  _k_sched_unlock();
}

// Compare thread priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static int
k_sched_priority_cmp(struct KThread *t1, struct KThread *t2)
{
  return t2->priority - t1->priority; 
}

// Check whether a reschedule is required (taking into account the priority
// of a thread most recently added to the run queue)
void
_k_sched_may_yield(struct KThread *candidate)
{
  struct KCpu *my_cpu;
  struct KThread *my_thread;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  my_cpu = k_cpu();
  my_thread = my_cpu->thread;

  if ((my_thread != NULL) && (k_sched_priority_cmp(candidate, my_thread) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      // Cannot yield right now, delay until the last call to sched_isr_exit()
      // or thread_unlock().
      my_thread->flags |= THREAD_FLAG_RESCHEDULE;
    } else {
      _k_sched_enqueue(my_thread);
      _k_sched_yield();
    }
  }
}

/**
 * Put the current thread into sleep.
 *
 * @param queue An optional queue to insert the thread into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
int
_k_sched_sleep(struct ListLink *queue, int interruptible, unsigned long timeout,
               struct KSpinLock *lock)
{
  struct KThread *my_thread = k_thread_current();

  if (lock != NULL) {
    _k_sched_lock();
    k_spinlock_release(lock);
  }

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  if (timeout != 0) {
    my_thread->timer.remain = timeout;
    k_timer_start(&my_thread->timer);
  }

  my_thread->state = interruptible
    ? THREAD_STATE_SLEEPING_INTERRUPTILE
    : THREAD_STATE_SLEEPING;

  if (queue != NULL)
    list_add_back(queue, &my_thread->link);

  _k_sched_yield();

  if (timeout != 0)
    k_timer_stop(&my_thread->timer);

  // someone may call this function while holding _k_sched_spinlock?
  if (lock != NULL) {
    _k_sched_unlock();
    k_spinlock_acquire(lock);
  }

  return my_thread->sleep_result;
}

void
_k_sched_resume(struct KThread *thread, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");
  
  if ((thread->state != THREAD_STATE_SLEEPING) &&
      (thread->state != THREAD_STATE_SLEEPING_INTERRUPTILE))
    panic("thread is not sleeping");

  thread->sleep_result = result;

  list_remove(&thread->link);

  _k_sched_enqueue(thread);
  _k_sched_may_yield(thread);
}

void
_k_sched_wakeup_all(struct ListLink *thread_list, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");
  
  while (!list_empty(thread_list)) {
    struct ListLink *link = thread_list->next;
    struct KThread *thread = LIST_CONTAINER(link, struct KThread, link);

    _k_sched_resume(thread, result);
  }
}

/**
 * Wake up the thread with the highest priority.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
_k_sched_wakeup_one(struct ListLink *queue, int result)
{
  struct ListLink *l;
  struct KThread *highest;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");

  highest = NULL;

  LIST_FOREACH(queue, l) {
    struct KThread *t = LIST_CONTAINER(l, struct KThread, link);
    
    if ((highest == NULL) || (k_sched_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL)
    _k_sched_resume(highest, result);
}
