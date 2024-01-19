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
#include <kernel/mm/vm.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/kmem.h>
#include <kernel/mm/page.h>

static void thread_run(void);

/**
 * Resume execution of a previously suspended thread (or begin execution of a
 * newly created one).
 * 
 * @param thread The kernel thread to resume execution
 */
int
thread_resume(struct Thread *thread)
{
  sched_lock();

  if (thread->state != THREAD_STATE_SUSPENDED) {
    sched_unlock();
    return -EINVAL;
  }

  sched_enqueue(thread);
  sched_may_yield(thread);

  sched_unlock();

  return 0;
}

/**
 * Relinguish the CPU allowing another thread to run.
 */
void
thread_yield(void)
{
  struct Thread *current = thread_current();
  
  if (current == NULL)
    panic("no current thread");

  sched_lock();

  sched_enqueue(current);
  sched_yield();

  sched_unlock();
}

// Execution of each thread begins here.
static void
thread_run(void)
{
  struct Thread *my_thread = thread_current();

  // Still holding the scheduler lock (acquired in sched_start)
  sched_unlock();

  // Make sure IRQs are enabled
  cpu_irq_enable();

  // Jump to the thread entry point
  my_thread->entry(my_thread->arg);

  // Destroy the thread on exit
  thread_exit();
}

static void
arch_thread_init_stack(struct Thread *thread)
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
  thread->context->lr = (uint32_t) thread_run;
}

static void
thread_timeout_callback(void *arg)
{
  struct Thread *thread = (struct Thread *) arg;

  sched_lock();

  if (thread->state == THREAD_STATE_SLEEPING) {
    thread->sleep_result = -ETIMEDOUT;

    list_remove(&thread->link);

    sched_enqueue(thread);
    sched_may_yield(thread);
  }

  sched_unlock();
}

/**
 * Initialize the kernel thread. After successful initialization, the thread
 * is placed into suspended state and must be explicitly made runnable by a call
 * to thread_resume().
 * 
 * @param process  Pointer to a process the thread belongs to.
 * @param thread   Pointer to the kernel thread to be initialized.
 * @param entry    thread entry point function.
 * @param priority thread priority value.
 * @param stack    Top of the thread stack.
 * 
 * @return 0 on success.
 */
struct Thread *
thread_create(struct Process *process, void (*entry)(void *), void *arg,
              int priority)
{
  struct Page *stack_page;
  struct Thread *thread;
  uint8_t *stack;

  if ((thread = (struct Thread *) kmem_alloc(thread_cache)) == NULL)
    return NULL;

  if ((stack_page = page_alloc_one(0)) == NULL) {
    kmem_free(thread_cache, thread);
    return NULL;
  }

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  thread->flags    = 0;
  thread->priority = priority;
  thread->state    = THREAD_STATE_SUSPENDED;
  thread->entry    = entry;
  thread->arg      = arg;
  thread->err      = 0;
  thread->process  = process;
  thread->kstack   = stack;
  thread->tf       = NULL;

  ktimer_create(&thread->timer, thread_timeout_callback, thread, 0, 0, 0);

  arch_thread_init_stack(thread);

  return thread;
}

/**
 * Destroy the specified thread
 */
void
thread_exit(void)
{
  struct Thread *thread = thread_current();

  if (thread == NULL)
    panic("no current thread");

  ktimer_destroy(&thread->timer);

  sched_lock();

  thread->state = THREAD_STATE_DESTROYED;

  sched_yield();

  panic("should not return");
}

/**
 * Get the currently executing thread.
 * 
 * @return A pointer to the currently executing thread or NULL
 */
struct Thread *
thread_current(void)
{
  struct Thread *thread;

  cpu_irq_save();
  thread = cpu_current()->thread;
  cpu_irq_restore();

  return thread;
}
