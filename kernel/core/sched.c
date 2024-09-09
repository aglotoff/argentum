#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/console.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/thread.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/vm.h>
#include <kernel/vm.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

#include "core_private.h"

void k_arch_switch(struct Context **, struct Context *);

KLIST_DECLARE(threads_to_destroy);
static struct KListLink sched_queue[THREAD_MAX_PRIORITIES];
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
    k_list_init(&sched_queue[i]);
}

// Add the specified thread to the run queue with the corresponding priority
void
_k_sched_enqueue(struct KThread *th)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  th->state = THREAD_STATE_READY;
  k_list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority thread from the run queue
static struct KThread *
k_sched_dequeue(void)
{
  struct KListLink *link;
  int i;
  
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  for (i = 0; i < THREAD_MAX_PRIORITIES; i++) {
    if (!k_list_is_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      k_list_remove(link);

      return KLIST_CONTAINER(link, struct KThread, link);
    }
  }

  return NULL;
}

static void
k_sched_switch(struct KThread *thread)
{
  struct KCpu *my_cpu = _k_cpu();

  if (thread->process != NULL)
    vm_arch_load(thread->process->vm->pgtab);

  thread->state = THREAD_STATE_RUNNING;

  thread->cpu = my_cpu;
  my_cpu->thread = thread;

  if (thread->context->lr < VIRT_KERNEL_BASE)
    panic("bad PC");

  k_arch_switch(&my_cpu->sched_context, thread->context);

  if ((intptr_t) thread->context - (intptr_t) thread->kstack < 64)
    panic("stack underflow %p %p", thread->context, thread->kstack);

  my_cpu->thread = NULL;
  thread->cpu = NULL;

  if (thread->process != NULL)
    vm_arch_load_kernel();
}

static void
k_sched_idle(void)
{
  // Cleanup destroyed threads
  while (!k_list_is_empty(&threads_to_destroy)) {
    struct KThread *thread;
    struct Page *kstack_page;
    
    thread = KLIST_CONTAINER(threads_to_destroy.next, struct KThread, link);

    k_list_remove(&thread->link);

    _k_sched_unlock();

    // Free the thread kernel stack
    kstack_page = kva2page(thread->kstack);
    kstack_page->ref_count--;
    assert(kstack_page->ref_count == 0);
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
_k_sched_yield_locked(void)
{
  int irq_flags;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  irq_flags = _k_cpu()->irq_flags;
  k_arch_switch(&k_thread_current()->context, _k_cpu()->sched_context);
  _k_cpu()->irq_flags = irq_flags;
}

void
_k_sched_add(struct KListLink *queue, struct KThread *thread)
{
  struct KListLink *l;
  struct KThread *other_thread;

  for (l = queue->next; l != queue; l = l->next) {
    other_thread = KLIST_CONTAINER(l, struct KThread, link);
    if (_k_sched_priority_cmp(thread, other_thread) > 0)
      break;
  }

  k_list_add_back(l, &thread->link);
}

/**
 * Put the current thread into sleep.
 *
 * @param queue An optional queue to insert the thread into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
int
_k_sched_sleep(struct KListLink *queue, int state, unsigned long timeout,
               struct KSpinLock *lock)
{
  struct KCpu *my_cpu;
  struct KThread *my_thread;

  if (lock != NULL) {
    _k_sched_lock();
    k_spinlock_release(lock);
  }

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");

  my_cpu = _k_cpu();
  my_thread = my_cpu->thread;

  if (my_cpu->lock_count > 0)
    panic("called from an IRQ context");
  if (my_cpu->thread == NULL)
    panic("called not by a thread");

  if (timeout != 0)
    _k_timer_start(&my_thread->timer, timeout);

  my_thread->state = state;

  if (queue != NULL)
    _k_sched_add(queue, my_thread);

  _k_sched_yield_locked();

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
_k_sched_raise_priority(struct KThread *thread, int priority)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");
  if (thread->priority <= priority)
    panic("new priority must be higher");

  thread->priority = priority;

  // TODO: change priorities for all owned mutexes

  switch (thread->state) {
  case THREAD_STATE_READY:
    // Move into another run queue
    k_list_remove(&thread->link);
    _k_sched_enqueue(thread);
    break;
  case THREAD_STATE_MUTEX:
    k_list_remove(&thread->link);
    _k_sched_add(&thread->wait_mutex->queue, thread);

    _k_mutex_may_raise_priority(thread->wait_mutex, thread->priority);
    break;
  default:
    break;
  }
}

void
_k_sched_resume(struct KThread *thread, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");

  switch (thread->state) {
  case THREAD_STATE_SLEEP:
    k_list_remove(&thread->link);
    break;
  case THREAD_STATE_MUTEX:
    k_list_remove(&thread->link);
  
    // TODO: this may lead to decreasing mutex priority

    break;
  default:
    return;
  }

  k_list_remove(&thread->link);

  thread->sleep_result = result;

  _k_sched_enqueue(thread);
  _k_sched_may_yield(thread);
}

// Resume all threads waiting on the given queue
// The caller must be holding the scheduler lock
void
_k_sched_wakeup_all_locked(struct KListLink *queue, int result)
{
  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");

  while (!k_list_is_empty(queue)) {
    struct KListLink *link = queue->next;
    struct KThread *thread = KLIST_CONTAINER(link, struct KThread, link);

    _k_sched_resume(thread, result);
  }
}

// Resume and return the highest priority thread waiting on the given queue
// The caller must be holding the scheduler lock
struct KThread *
_k_sched_wakeup_one_locked(struct KListLink *queue, int result)
{
  struct KThread *thread;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("sched not locked");

  if (k_list_is_empty(queue))
    return NULL;

  thread = KLIST_CONTAINER(queue->next, struct KThread, link);



  _k_sched_resume(thread, result);

  return thread;
}

// Check whether a reschedule is required (taking into account the priority
// of a thread most recently added to the run queue)
void
_k_sched_may_yield(struct KThread *thread)
{
  struct KCpu *my_cpu;
  struct KThread *my_thread;

  if (!k_spinlock_holding(&_k_sched_spinlock))
    panic("scheduler not locked");



  my_cpu = _k_cpu();
  my_thread = my_cpu->thread;

  if ((my_thread != NULL) && (_k_sched_priority_cmp(thread, my_thread) > 0)) {
    if (my_cpu->lock_count > 0) {
      // Cannot yield right now, delay until the last call to k_irq_end()
      // or thread_unlock().
      my_thread->flags |= THREAD_FLAG_RESCHEDULE;
    } else {
      _k_sched_enqueue(my_thread);
      _k_sched_yield_locked();
    }
  }
}
