#include <assert.h>
#include <string.h>

#include <cprintf.h>
#include <cpu.h>
#include <list.h>
#include <mm/kobject.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <process.h>
#include <sync.h>
#include <kthread.h>

static struct KObjectPool *thread_pool;

// Run queue
static struct {
  struct ListLink run_queue;
  struct SpinLock lock;
} sched;

static void scheduler_yield(void);

void
kthread_free(struct KThread *thread)
{
  if (thread->process != NULL) {
    struct Page *kstack_page;

    // Free the kernel stack
    kstack_page = kva2page(thread->process->kstack);
    kstack_page->ref_count--;
    page_free_one(kstack_page);
  }

  kobject_free(thread_pool, thread);
}

void context_switch(struct Context **, struct Context *);

void
scheduler_init(void)
{
  thread_pool = kobject_pool_create("thread_pool", sizeof(struct KThread), 0);
  if (thread_pool == NULL)
    panic("cannot allocate thread pool");

  list_init(&sched.run_queue);
  spin_init(&sched.lock, "sched");
}

void
scheduler_start(void)
{
  struct ListLink *link;
  struct KThread *next;

  for (;;) {
    irq_enable();

    spin_lock(&sched.lock);

    while (!list_empty(&sched.run_queue)) {
      link = sched.run_queue.next;
      list_remove(link);

      next = LIST_CONTAINER(link, struct KThread, link);
      assert(next->state == KTHREAD_RUNNABLE);

      next->state = KTHREAD_RUNNING;
      my_cpu()->thread = next;

      if (next->process != NULL)
        mmu_switch_user(next->process->vm->trtab);

      context_switch(&my_cpu()->scheduler, next->context);

      if (next->process != NULL)
        mmu_switch_kernel();

      if (next->state == KTHREAD_DESTROYED)
        kthread_free(next);
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

struct KThread *
kthread_create(struct Process *process, void (*entry)(void), uint8_t *stack)
{
  struct KThread *thread;

  if ((thread = (struct KThread *) kobject_alloc(thread_pool)) == NULL)
    return NULL;

  stack -= sizeof *thread->context;
  thread->context = (struct Context *) stack;
  memset(thread->context, 0, sizeof *thread->context);
  thread->context->lr = (uint32_t) kthread_run;
  thread->entry = entry;

  thread->process = process;

  return thread;
}

void
kthread_destroy(struct KThread *thread)
{
  spin_lock(&sched.lock);

  thread->state = KTHREAD_DESTROYED;

  scheduler_yield();

  panic("should not return");
}

void
kthread_yield(void)
{
  struct KThread *current = my_thread();
  
  spin_lock(&sched.lock);

  current->state = KTHREAD_RUNNABLE;
  list_add_back(&sched.run_queue, &current->link);

  // Return into the scheduler loop
  scheduler_yield();

  spin_unlock(&sched.lock);
}

// A process' very first scheduling by scheduler() will switch here.
void
kthread_run(void)
{
  // Still holding the process table lock.
  spin_unlock(&sched.lock);

  my_thread()->entry();
}

/**
 * 
 */
void
kthread_sleep(struct ListLink *wait_queue, struct SpinLock *lock)
{
  struct KThread *current = my_thread();

  if (lock != &sched.lock) {
    spin_lock(&sched.lock);
    spin_unlock(lock);
  }

  list_add_back(wait_queue, &current->link);

  current->state = KTHREAD_NOT_RUNNABLE;

  scheduler_yield();

  if (lock != &sched.lock) {
    spin_unlock(&sched.lock);
    spin_lock(lock);
  }
}

void
kthread_enqueue(struct KThread *th)
{
  spin_lock(&sched.lock);

  th->state = KTHREAD_RUNNABLE;
  list_add_back(&sched.run_queue, &th->link);

  spin_unlock(&sched.lock);
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup(struct ListLink *wait_queue)
{
  struct ListLink *l;
  struct KThread *t;

  spin_lock(&sched.lock);

  while (!list_empty(wait_queue)) {
    l = wait_queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct KThread, link);

    t->state = KTHREAD_RUNNABLE;
    list_add_back(&sched.run_queue, &t->link);
  }

  spin_unlock(&sched.lock);
}
