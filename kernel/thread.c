#include <assert.h>
#include <string.h>

#include <kernel/cpu.h>
#include <kernel/list.h>
#include <kernel/process.h>
#include <kernel/sync.h>
#include <kernel/thread.h>

// Run queue
static struct {
  struct ListLink run_queue;
  struct SpinLock lock;
} sched;

static void thread_suspend(int);
static void thread_resume(struct Thread *);
static void scheduler_yield(void);

void context_switch(struct Context **, struct Context *);

struct Thread *
my_thread(void)
{
  struct Thread *thread;

  irq_save();
  thread = my_cpu()->thread;
  irq_restore();

  return thread;
}

void
scheduler_init(void)
{
  list_init(&sched.run_queue);
  spin_init(&sched.lock, "sched");
}

void
scheduler_start(void)
{
  struct ListLink *link;
  struct Thread *next;

  //cprintf("Test %f %d %f\n", 23.456, 45, 78.8);

  for (;;) {
    irq_enable();

    spin_lock(&sched.lock);

    while (!list_empty(&sched.run_queue)) {
      link = sched.run_queue.next;
      list_remove(link);

      next = LIST_CONTAINER(link, struct Thread, link);
      assert(next->state == THREAD_RUNNABLE);

      next->state = THREAD_RUNNING;
      my_cpu()->thread = next;

      if (next->process != NULL)
        vm_switch_user(next->process->vm.trtab);

      context_switch(&my_cpu()->scheduler, next->context);

      if (next->process != NULL)
        vm_switch_kernel();
    }

    // Mark that no process is running on this CPU.
    my_cpu()->thread = NULL;

    spin_unlock(&sched.lock);

    asm volatile("wfi");
  }
}

static void
scheduler_yield(void)
{
  int irq_flags;

  irq_flags = my_cpu()->irq_flags;
  context_switch(&my_thread()->context, my_cpu()->scheduler);
  my_cpu()->irq_flags = irq_flags;
}

void
thread_init(struct Thread *thread, void (*entry)(void), uint8_t *stack,
            struct Process *process)
{
  stack -= sizeof *thread->context;
  thread->context = (struct Context *) stack;
  memset(thread->context, 0, sizeof *thread->context);
  thread->context->lr = (uint32_t) thread_run;
  thread->entry = entry;

  thread->state = THREAD_EMBRIO;

  thread->process = process;
}

void
thread_yield(void)
{
  struct Thread *current = my_thread();
  
  spin_lock(&sched.lock);

  current->state = THREAD_RUNNABLE;
  list_add_back(&sched.run_queue, &current->link);

  // Return into the scheduler loop
  scheduler_yield();

  spin_unlock(&sched.lock);
}

/**
 * Suspend execution of the current process. The caller must hold the
 * sched.lock.
 */
static void
thread_suspend(int state)
{
  assert(spin_holding(&sched.lock));

  my_thread()->state = state;

  scheduler_yield();
}

/**
 * Resume execution of the previously suspended process.
 * 
 * @param proc The process to be resumed.
 */
static void
thread_resume(struct Thread *th)
{
  assert(spin_holding(&sched.lock));
  assert(th->state == THREAD_SLEEPING);

  th->state = THREAD_RUNNABLE;

  list_remove(&th->link);
  list_add_back(&sched.run_queue, &th->link);
}

// A process' very first scheduling by scheduler() will switch here.
void
thread_run(void)
{
  // Still holding the process table lock.
  spin_unlock(&sched.lock);

  my_thread()->entry();
}

/**
 * 
 */
void
thread_sleep(struct ListLink *wait_queue, struct SpinLock *lock, int state)
{
  struct Thread *current = my_thread();

  if (lock != &sched.lock) {
    spin_lock(&sched.lock);
    spin_unlock(lock);
  }

  list_add_back(wait_queue, &current->link);
  thread_suspend(state);

  if (lock != &sched.lock) {
    spin_unlock(&sched.lock);
    spin_lock(lock);
  }
}

void
thread_enqueue(struct Thread *th)
{
  spin_lock(&sched.lock);

  th->state = THREAD_RUNNABLE;
  list_add_back(&sched.run_queue, &th->link);

  spin_unlock(&sched.lock);
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
thread_wakeup(struct ListLink *wait_queue)
{
  struct ListLink *l;
  struct Thread *t;

  spin_lock(&sched.lock);

  while (!list_empty(wait_queue)) {
    l = wait_queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct Thread, link);
    thread_resume(t);
  }

  spin_unlock(&sched.lock);
}
