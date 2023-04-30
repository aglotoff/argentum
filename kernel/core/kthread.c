#include <assert.h>
#include <errno.h>
#include <string.h>

#include <argentum/cprintf.h>
#include <argentum/cpu.h>
#include <argentum/irq.h>
#include <argentum/kthread.h>
#include <argentum/list.h>
#include <argentum/mm/kmem.h>
#include <argentum/mm/mmu.h>
#include <argentum/mm/page.h>
#include <argentum/process.h>
#include <argentum/spinlock.h>

static struct ListLink run_queue[KTHREAD_MAX_PRIORITIES];
struct SpinLock __sched_lock;

struct KThread *
kthread_current(void)
{
  struct KThread *thread;

  cpu_irq_save();
  thread = cpu_current()->thread;
  cpu_irq_restore();

  return thread;
}

void context_switch(struct Context **, struct Context *);

void
sched_init(void)
{
  int i;

  for (i = 0; i < KTHREAD_MAX_PRIORITIES; i++)
    list_init(&run_queue[i]);

  spin_init(&__sched_lock, "sched");
}

void
kthread_enqueue(struct KThread *th)
{
  if (!sched_locked())
    panic("scheduler not locked");

  th->state = KTHREAD_READY;
  list_add_back(&run_queue[th->priority], &th->link);
}

static struct KThread *
kthread_list_remove(void)
{
  struct ListLink *link;
  int i;
  
  assert(sched_locked());

  for (i = 0; i < KTHREAD_MAX_PRIORITIES; i++) {
    if (!list_empty(&run_queue[i])) {
      link = run_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct KThread, link);
    }
  }

  return NULL;
}

void
sched_start(void)
{
  struct KThread *next;

  sched_lock();

  for (;;) {
    next = kthread_list_remove();

    if (next != NULL) {
      assert(next->state == KTHREAD_READY);

      next->state = KTHREAD_RUNNING;
      cpu_current()->thread = next;

      if (next->process != NULL)
        mmu_switch_user(next->process->vm->trtab);

      context_switch(&cpu_current()->scheduler, next->context);

      // Mark that no process is running on this CPU.
      cpu_current()->thread = NULL;

      if (next->process != NULL) {
        mmu_switch_kernel();

        if (next->state == KTHREAD_DESTROYED) {
          sched_unlock();
          process_thread_free(next);
          sched_lock();
        }
      }
    } else {
      

      sched_unlock();

      cpu_irq_enable();

      asm volatile("wfi");

      sched_lock();
    }
  }
}

void
sched_yield(void)
{
  int irq_flags;

  assert(sched_locked());

  irq_flags = cpu_current()->irq_flags;
  context_switch(&kthread_current()->context, cpu_current()->scheduler);
  cpu_current()->irq_flags = irq_flags;
}

int
kthread_create(struct Process *process, struct KThread *thread, void (*entry)(void), int priority, uint8_t *stack)
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

void
kthread_destroy(struct KThread *thread)
{
  sched_lock();

  thread->state = KTHREAD_DESTROYED;

  sched_yield();

  panic("should not return");
}

void
kthread_yield(void)
{
  struct KThread *current = kthread_current();
  
  sched_lock();

  kthread_enqueue(current);
  sched_yield();

  sched_unlock();
}

// A process' very first scheduling by scheduler() will switch here.
void
kthread_run(void)
{
  // Still holding the scheduler lock.
  sched_unlock();

  cpu_irq_enable();

  kthread_current()->entry();
}

/**
 * 
 */
void
kthread_sleep(struct ListLink *queue, int state, struct SpinLock *lock)
{
  struct KThread *current = kthread_current();

  if (lock != &__sched_lock) {
    sched_lock();
    if (lock != NULL)
      spin_unlock(lock);
  }

  if (!sched_locked())
    panic("scheduler not locked");

  current->state = state;
  list_add_back(queue, &current->link);

  sched_yield();

  if (lock != &__sched_lock) {
    sched_unlock();
    if (lock != NULL)
      spin_lock(lock);
  }
}

int
kthread_priority_cmp(struct KThread *t1, struct KThread *t2)
{
  return t2->priority - t1->priority; 
}

static void
kthread_check_resched(struct KThread *t)
{
  struct Cpu *my_cpu;
  struct KThread *my_thread;

  my_cpu = cpu_current();
  my_thread = my_cpu->thread;
  
  if ((my_thread != NULL) && (kthread_priority_cmp(t, my_thread) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      my_thread->flags |= KTHREAD_RESCHEDULE;
    } else {
      kthread_enqueue(my_thread);
      sched_yield();
    }
  }
}

int
kthread_resume(struct KThread *t)
{
  sched_lock();

  if (t->state != KTHREAD_SUSPENDED) {
    sched_unlock();
    return -EINVAL;
  }

  kthread_enqueue(t);
  kthread_check_resched(t);

  sched_unlock();

  return 0;
}

/**
 * Wake up the thread with the highest priority.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup_one(struct ListLink *queue)
{
  struct ListLink *l;
  struct KThread *highest;

  highest = NULL;

  sched_lock();

  LIST_FOREACH(queue, l) {
    struct KThread *t = LIST_CONTAINER(l, struct KThread, link);
    
    if ((highest == NULL) || (kthread_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    list_remove(&highest->link);
    kthread_enqueue(highest);

    kthread_check_resched(highest);
  }

  sched_unlock();
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup_all(struct ListLink *queue)
{
  sched_lock();

  while (!list_empty(queue)) {
    struct ListLink *l;
    struct KThread *t;

    l = queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct KThread, link);
    kthread_enqueue(t);

    kthread_check_resched(t);
  }

  sched_unlock();
}
