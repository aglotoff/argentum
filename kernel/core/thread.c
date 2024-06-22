#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/thread.h>
#include <kernel/spinlock.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/vm.h>
#include <kernel/vm.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

#include "core_private.h"

static void k_thread_run(void);

/**
 * Resume execution of a previously suspended thread (or begin execution of a
 * newly created one).
 * 
 * @param thread The kernel thread to resume execution
 */
int
k_thread_resume(struct KThread *thread)
{
  _k_sched_lock();

  if (thread->state != THREAD_STATE_SUSPENDED) {
    _k_sched_unlock();
    return -EINVAL;
  }

  _k_sched_enqueue(thread);
  _k_sched_may_yield(thread);

  _k_sched_unlock();

  return 0;
}

/**
 * Relinguish the CPU allowing another thread to run.
 */
void
k_thread_yield(void)
{
  struct KThread *current = k_thread_current();
  
  if (current == NULL)
    panic("no current thread");

  _k_sched_lock();

  _k_sched_enqueue(current);
  _k_sched_yield_locked();

  _k_sched_unlock();
}

// Execution of each thread begins here.
static void
k_thread_run(void)
{
  struct KThread *my_thread = k_thread_current();

  // Still holding the scheduler lock (acquired in k_sched_start)
  _k_sched_unlock();

  k_irq_enable();

  my_thread->entry(my_thread->arg);

  k_thread_exit();
}

static void
arch_thread_init_stack(struct KThread *thread)
{
  uint8_t *sp = (uint8_t *) thread->kstack + PAGE_SIZE;

  // Allocate space for user-mode trap frame
  if (thread->process != NULL) {
    sp -= sizeof(struct TrapFrame);
    thread->tf = (struct TrapFrame *) sp;
    memset(thread->tf, 0, sizeof(struct TrapFrame));
  }

  // Initialize the kernel-mode thread context
  sp -= sizeof(struct Context);
  thread->context = (struct Context *) sp;
  memset(thread->context, 0, sizeof(struct Context));
  thread->context->lr = (uint32_t) k_thread_run;
}

static void
k_thread_timeout_callback(void *arg)
{
  struct KThread *thread = (struct KThread *) arg;

  _k_sched_lock();

  switch (thread->state) {
  case THREAD_STATE_MUTEX:
    // TODO
    // fall through
  case THREAD_STATE_SLEEP:
  case THREAD_STATE_SEMAPHORE:
    _k_sched_resume(thread, -ETIMEDOUT);
    break;
  }

  _k_sched_unlock();
}

void
k_thread_interrupt(struct KThread *thread)
{
  _k_sched_lock();

  // TODO: if thread is running, send an SGI

  _k_sched_resume(thread, -EINTR);

  _k_sched_unlock();
}

/**
 * Initialize the kernel thread. After successful initialization, the thread
 * is placed into suspended state and must be explicitly made runnable by a call
 * to k_thread_resume().
 * 
 * @param process  Pointer to a process the thread belongs to.
 * @param thread   Pointer to the kernel thread to be initialized.
 * @param entry    thread entry point function.
 * @param priority thread priority value.
 * @param stack    Top of the thread stack.
 * 
 * @return 0 on success.
 */
struct KThread *
k_thread_create(struct Process *process, void (*entry)(void *), void *arg,
                int priority)
{
  struct Page *stack_page;
  struct KThread *thread;
  uint8_t *stack;

  if ((thread = (struct KThread *) k_object_pool_get(thread_cache)) == NULL)
    return NULL;

  if ((stack_page = page_alloc_one(0)) == NULL) {
    k_object_pool_put(thread_cache, thread);
    return NULL;
  }

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  k_list_init(&thread->mutex_list);
  thread->flags    = 0;
  thread->priority = priority;
  thread->state    = THREAD_STATE_SUSPENDED;
  thread->entry    = entry;
  thread->arg      = arg;
  thread->err      = 0;
  thread->process  = process;
  thread->kstack   = stack;
  thread->tf       = NULL;

  k_timer_create(&thread->timer, k_thread_timeout_callback, thread, 0, 0, 0);

  arch_thread_init_stack(thread);

  return thread;
}

/**
 * Destroy the specified thread
 */
void
k_thread_exit(void)
{
  struct KThread *thread = k_thread_current();
  extern struct KListLink threads_to_destroy;

  if (thread == NULL)
    panic("no current thread");

  k_timer_destroy(&thread->timer);

  _k_sched_lock();

  thread->state = THREAD_STATE_DESTROYED;
  k_list_add_back(&threads_to_destroy, &thread->link);

  _k_sched_yield_locked();

  panic("should not return");
}

/**
 * Get the currently executing thread.
 * 
 * @return A pointer to the currently executing thread or NULL
 */
struct KThread *
k_thread_current(void)
{
  struct KThread *thread;

  k_irq_save();
  thread = _k_cpu()->thread;
  k_irq_restore();

  return thread;
}
