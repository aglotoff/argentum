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
#include <kernel/object_pool.h>
#include <kernel/mm/page.h>

void arch_context_switch(struct Context **, struct Context *);

static struct ListLink sched_queue[THREAD_MAX_PRIORITIES];
struct SpinLock __sched_lock = SPIN_INITIALIZER("sched");

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel threads.
 */
void
sched_init(void)
{
  int i;

  thread_cache = object_pool_create("thread_cache", sizeof(struct Thread), 0, 0,
                                   NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread cache");

  for (i = 0; i < THREAD_MAX_PRIORITIES; i++)
    list_init(&sched_queue[i]);
}

// Add the specified thread to the run queue with the corresponding priority
void
sched_enqueue(struct Thread *th)
{
  if (!spin_holding(&__sched_lock))
    panic("scheduler not locked");

  th->state = THREAD_STATE_READY;
  list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority thread from the run queue
static struct Thread *
sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  if (!spin_holding(&__sched_lock))
    panic("scheduler not locked");

  for (i = 0; i < THREAD_MAX_PRIORITIES; i++) {
    if (!list_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct Thread, link);
    }
  }

  return NULL;
}

/**
 * Start the scheduler main loop. This function never returns.
 */
void
sched_start(void)
{
  struct Cpu *my_cpu;

  sched_lock();

  my_cpu = cpu_current();

  for (;;) {
    struct Thread *next = sched_dequeue();

    if (next != NULL) {
      assert(next->state == THREAD_STATE_READY);

      if (next->process != NULL)
        vm_load(next->process->vm->pgdir);

      next->state = THREAD_STATE_RUNNING;

      next->cpu = my_cpu;
      my_cpu->thread = next;

      arch_context_switch(&my_cpu->sched_context, next->context);

      my_cpu->thread = NULL;
      next->cpu = NULL;

      if (next->process != NULL)
        vm_load_kernel();

      // Perform cleanup for the exited thread
      if (next->state == THREAD_STATE_DESTROYED) {
        struct Page *kstack_page;

        next->state = THREAD_STATE_NONE;
  
        sched_unlock();

        // Free the kernel stack
        kstack_page = kva2page(next->kstack);
        kstack_page->ref_count--;
        page_free_one(kstack_page);

        object_pool_put(thread_cache, next);

        sched_lock();
      }
    } else {
      sched_unlock();

      cpu_irq_enable();
      asm volatile("wfi");

      sched_lock();
    }
  }
}

// Switch back from the current thread context back to the scheduler loop
void
sched_yield(void)
{
  int irq_flags;

  if (!spin_holding(&__sched_lock))
    panic("scheduler not locked");

  irq_flags = cpu_current()->irq_flags;
  arch_context_switch(&thread_current()->context, cpu_current()->sched_context);
  cpu_current()->irq_flags = irq_flags;
}

/**
 * Notify the kernel that an ISR processing has started.
 */
void
sched_isr_enter(void)
{
  cpu_current()->isr_nesting++;
}

/**
 * Notify the kernel that an ISR processing is finished.
 */
void
sched_isr_exit(void)
{
  struct Cpu *my_cpu;

  sched_lock();

  my_cpu = cpu_current();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct Thread *my_thread = my_cpu->thread;

    // Before resuming the current thread, check whether it must give up the CPU
    // or exit.
    if ((my_thread != NULL) && (my_thread->flags & THREAD_FLAG_RESCHEDULE)) {
      my_thread->flags &= ~THREAD_FLAG_RESCHEDULE;

      sched_enqueue(my_thread);
      sched_yield();
    }
  }

  sched_unlock();
}

// Compare thread priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static int
sched_priority_cmp(struct Thread *t1, struct Thread *t2)
{
  return t2->priority - t1->priority; 
}

// Check whether a reschedule is required (taking into account the priority
// of a thread most recently added to the run queue)
void
sched_may_yield(struct Thread *candidate)
{
  struct Cpu *my_cpu;
  struct Thread *my_thread;

  if (!spin_holding(&__sched_lock))
    panic("scheduler not locked");

  my_cpu = cpu_current();
  my_thread = my_cpu->thread;

  if ((my_thread != NULL) && (sched_priority_cmp(candidate, my_thread) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      // Cannot yield right now, delay until the last call to sched_isr_exit()
      // or thread_unlock().
      my_thread->flags |= THREAD_FLAG_RESCHEDULE;
    } else {
      sched_enqueue(my_thread);
      sched_yield();
    }
  }
}

void
sched_wakeup_all(struct ListLink *thread_list, int result)
{
  if (!spin_holding(&__sched_lock))
    panic("sched not locked");
  
  while (!list_empty(thread_list)) {
    struct ListLink *link = thread_list->next;
    struct Thread *thread = LIST_CONTAINER(link, struct Thread, link);

    list_remove(link);

    thread->sleep_result = result;

    sched_enqueue(thread);
    sched_may_yield(thread);
  }
}

/**
 * Wake up the thread with the highest priority.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
sched_wakeup_one(struct ListLink *queue, int result)
{
  struct ListLink *l;
  struct Thread *highest;

  if (!spin_holding(&__sched_lock))
    panic("sched not locked");

  highest = NULL;

  LIST_FOREACH(queue, l) {
    struct Thread *t = LIST_CONTAINER(l, struct Thread, link);
    
    if ((highest == NULL) || (sched_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    // cprintf("wakeup %p from %p\n", highest, queue);

    list_remove(&highest->link);
    highest->sleep_result = result;

    sched_enqueue(highest);
    sched_may_yield(highest);
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
sched_sleep(struct ListLink *queue, unsigned long timeout,
            struct SpinLock *lock)
{
  struct Thread *my_thread = thread_current();

  if (lock != NULL) {
    sched_lock();
    spin_unlock(lock);
  }

  if (!spin_holding(&__sched_lock))
    panic("scheduler not locked");

  if (timeout != 0) {
    my_thread->timer.remain = timeout;
    ktimer_start(&my_thread->timer);
  }

  my_thread->state = THREAD_STATE_SLEEPING;

  if (queue != NULL)
    list_add_back(queue, &my_thread->link);

  sched_yield();

  if (timeout != 0)
    ktimer_stop(&my_thread->timer);

  // someone may call this function while holding __sched_lock?
  if (lock != NULL) {
    sched_unlock();
    spin_lock(lock);
  }

  return my_thread->sleep_result;
}
