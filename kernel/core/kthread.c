#include <assert.h>
#include <errno.h>
#include <string.h>

#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/irq.h>
#include <argentum/kthread.h>
#include <argentum/mm/kmem.h>
#include <argentum/mm/mmu.h>
#include <argentum/mm/page.h>
#include <argentum/process.h>
#include <argentum/spinlock.h>

static void kthread_run(void);
void context_switch(struct Context **, struct Context *);

static struct ListLink run_queue[KTHREAD_MAX_PRIORITIES];
struct SpinLock sched_lock;

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel threads.
 */
void
sched_init(void)
{
  int i;

  for (i = 0; i < KTHREAD_MAX_PRIORITIES; i++)
    list_init(&run_queue[i]);

  spin_init(&sched_lock, "sched");
}

// Add the specified task to the run queue with the corresponding priority
static void
sched_enqueue(struct KThread *th)
{
  if (!spin_holding(&sched_lock))
    panic("scheduler not locked");

  th->state = KTHREAD_READY;
  list_add_back(&run_queue[th->priority], &th->link);
}

// Retrieve the highest-priority thread from the run queue
static struct KThread *
sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  assert(spin_holding(&sched_lock));

  for (i = 0; i < KTHREAD_MAX_PRIORITIES; i++) {
    if (!list_empty(&run_queue[i])) {
      link = run_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct KThread, link);
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

  spin_lock(&sched_lock);

  my_cpu = cpu_current();

  for (;;) {
    struct KThread *next = sched_dequeue();

    if (next != NULL) {
      assert(next->state == KTHREAD_READY);

      if (next->process != NULL)
        mmu_switch_user(next->process->vm->trtab);

      next->state = KTHREAD_RUNNING;
      my_cpu->thread = next;

      context_switch(&my_cpu->scheduler, next->context);

      my_cpu->thread = NULL;

      if (next->process != NULL) {
        mmu_switch_kernel();

        if (next->state == KTHREAD_DESTROYED) {
          spin_unlock(&sched_lock);
          process_thread_free(next);
          spin_lock(&sched_lock);
        }
      }
    } else {
      spin_unlock(&sched_lock);

      cpu_irq_enable();
      asm volatile("wfi");

      spin_lock(&sched_lock);
    }
  }
}

// Switch back from the current thread context back to the scheduler loop
static void
sched_yield(void)
{
  int irq_flags;

  assert(spin_holding(&sched_lock));

  irq_flags = cpu_current()->irq_flags;
  context_switch(&kthread_current()->context, cpu_current()->scheduler);
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

  spin_lock(&sched_lock);

  my_cpu = cpu_current();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct KThread *my_thread = my_cpu->thread;

    if (my_thread != NULL) {
      // Before resuming the current thread, check whether it must give up the
      // CPU due to a higher-priority thread becoming available or due to time
      // quanta exhaustion.
      if (my_thread->flags & KTHREAD_RESCHEDULE) {
        my_thread->flags &= ~KTHREAD_RESCHEDULE;

        sched_enqueue(my_thread);
        sched_yield();
      }

      // TODO: the thread could also be marked for desruction?
    }
  }

  spin_unlock(&sched_lock);
}

/**
 * Notify the kernel that a timer IRQ has occured.
 */
void
sched_tick(void)
{
  struct KThread *current_thread = kthread_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for more complex sheculing policies
  if (current_thread != NULL) {
    spin_lock(&sched_lock);
    current_thread->flags |= KTHREAD_RESCHEDULE;
    spin_unlock(&sched_lock);
  }
}

/**
 * Get the currently executing thread.
 * 
 * @return A pointer to the currently executing thread or NULL
 */
struct KThread *
kthread_current(void)
{
  struct KThread *thread;

  cpu_irq_save();
  thread = cpu_current()->thread;
  cpu_irq_restore();

  return thread;
}

/**
 * Initialize the kernel thread. After successful initialization, the thread
 * is put into suspended state and must be explicitly made runnable by a call
 * to kthread_resume().
 * 
 * @param process  Pointer to a process the thread belongs to.
 * @param thread   Pointer to the kernel thread to be initialized.
 * @param entry    Thread entry point function.
 * @param priority Thread priority value.
 * @param stack    Top of the thread stack.
 * 
 * @return 0 on success.
 */
int
kthread_init(struct Process *process, struct KThread *thread,
             void (*entry)(void), int priority, uint8_t *stack)
{
  thread->flags = 0;
  thread->priority = priority;
  thread->state = KTHREAD_SUSPENDED;

  stack -= sizeof *thread->context;
  thread->context = (struct Context *) stack;
  memset(thread->context, 0, sizeof *thread->context);
  thread->context->lr = (uint32_t) kthread_run;
  thread->entry = entry;

  thread->process = process;
  return 0;
}

/**
 * Destroy the specified thread
 */
void
kthread_destroy(struct KThread *thread)
{
  struct KThread *my_thread = kthread_current();

  if (thread == NULL)
    thread = my_thread;

  spin_lock(&sched_lock);

  // TODO: state-specific cleanup
  // TODO: the thread may be executing on another CPU! send IPI in this case

  thread->state = KTHREAD_DESTROYED;

  if (thread == my_thread) {
    sched_yield();
    panic("should not return");
  }

  spin_unlock(&sched_lock);
}

/**
 * Relinguish the CPU allowing another thread to be run.
 */
void
kthread_yield(void)
{
  struct KThread *current = kthread_current();
  
  if (current == NULL)
    panic("no current thread");

  spin_lock(&sched_lock);

  sched_enqueue(current);
  sched_yield();

  spin_unlock(&sched_lock);
}

// Execution of each thread begins here.
static void
kthread_run(void)
{
  // Still holding the scheduler lock (acquired in sched_start)
  spin_unlock(&sched_lock);

  // Make sure IRQs are enabled
  cpu_irq_enable();

  // Jump to the thread entry point
  kthread_current()->entry();
}

// Compare thread priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static int
kthread_priority_cmp(struct KThread *t1, struct KThread *t2)
{
  return t2->priority - t1->priority; 
}

// Check whether a reschedule is required (taking into account the priority
// of a thread most recently added to the run queue)
static void
kthread_check_resched(struct KThread *recent)
{
  struct Cpu *my_cpu;
  struct KThread *my_thread;

  assert(spin_holding(&sched_lock));

  my_cpu = cpu_current();
  my_thread = my_cpu->thread;

  if ((my_thread != NULL) && (kthread_priority_cmp(recent, my_thread) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      // Cannot yield inside an ISR handler, delay until the last call to
      // sched_isr_exit()
      my_thread->flags |= KTHREAD_RESCHEDULE;
    } else {
      sched_enqueue(my_thread);
      sched_yield();
    }
  }
}

/**
 * Resume execution of a previously suspended thread (or begin execution of a
 * newly created one).
 * 
 * @param thread The kernel thread to resume execution
 */
int
kthread_resume(struct KThread *thread)
{
  spin_lock(&sched_lock);

  if (thread->state != KTHREAD_SUSPENDED) {
    spin_unlock(&sched_lock);
    return -EINVAL;
  }

  sched_enqueue(thread);
  kthread_check_resched(thread);

  spin_unlock(&sched_lock);

  return 0;
}

/**
 * Put the current thread into sleep.
 *
 * @param queue An optional queue to insert the thread into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
void
kthread_sleep(struct ListLink *queue, int state, struct SpinLock *lock)
{
  struct KThread *current = kthread_current();

  // someone may call this function while holding sched_lock?
  if (lock != &sched_lock) {
    spin_lock(&sched_lock);
    if (lock != NULL)
      spin_unlock(lock);
  }

  assert(spin_holding(&sched_lock));

  if (queue != NULL)
    list_add_back(queue, &current->link);

  current->state = state;
  sched_yield();

  // someone may call this function while holding sched_lock?
  if (lock != &sched_lock) {
    spin_unlock(&sched_lock);
    if (lock != NULL)
      spin_lock(lock);
  }
}

/**
 * Wake up the thread with the highest priority.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup_one(struct ListLink *queue)
{
  struct ListLink *l;
  struct KThread *highest;

  highest = NULL;

  spin_lock(&sched_lock);

  LIST_FOREACH(queue, l) {
    struct KThread *t = LIST_CONTAINER(l, struct KThread, link);
    
    if ((highest == NULL) || (kthread_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    list_remove(&highest->link);
    sched_enqueue(highest);

    kthread_check_resched(highest);
  }

  spin_unlock(&sched_lock);
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup_all(struct ListLink *queue)
{
  spin_lock(&sched_lock);

  while (!list_empty(queue)) {
    struct ListLink *l;
    struct KThread *t;

    l = queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct KThread, link);
    sched_enqueue(t);

    kthread_check_resched(t);
  }

  spin_unlock(&sched_lock);
}
