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

static struct KMemCache *thread_cache;
static struct ListLink run_queue[KTHREAD_MAX_PRIORITIES];

struct SpinLock __sched_lock;

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

  kmem_free(thread_cache, thread);
}

void context_switch(struct Context **, struct Context *);

void
sched_init(void)
{
  int i;

  thread_cache = kmem_cache_create("thread_cache", sizeof(struct KThread), 0, NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread cache");

  for (i = 0; i < KTHREAD_MAX_PRIORITIES; i++)
    list_init(&run_queue[i]);

  spin_init(&__sched_lock, "sched");
}

void
kthread_list_add(struct KThread *th)
{
  if (!sched_locked())
    panic("scheduler not locked");

  th->state = KTHREAD_RUNNABLE;
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
    } else {
      // Mark that no process is running on this CPU.
      my_cpu()->thread = NULL;

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

  irq_flags = my_cpu()->irq_flags;
  context_switch(&my_thread()->context, my_cpu()->scheduler);
  my_cpu()->irq_flags = irq_flags;
}

struct KThread *
kthread_create(struct Process *process, void (*entry)(void), int priority, uint8_t *stack)
{
  struct KThread *thread;

  if ((thread = (struct KThread *) kmem_alloc(thread_cache)) == NULL)
    return NULL;

  thread->flags = 0;
  thread->priority = priority;
  thread->state = KTHREAD_SUSPENDED;

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
  sched_lock();

  thread->state = KTHREAD_DESTROYED;

  sched_yield();

  panic("should not return");
}

void
kthread_yield(void)
{
  struct KThread *current = my_thread();
  
  sched_lock();

  kthread_list_add(current);
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

  my_thread()->entry();
}

/**
 * 
 */
void
kthread_sleep(struct ListLink *queue, int state)
{
  struct KThread *current = my_thread();

  if (!sched_locked())
    panic("scheduler not locked");

  current->state = state;
  list_add_back(queue, &current->link);

  sched_yield();
}

int
kthread_priority_cmp(struct KThread *t1, struct KThread *t2)
{
  return t2->priority - t1->priority; 
}

int
kthread_resume(struct KThread *t)
{
  // struct KThread *current = my_thread();

  sched_lock();

  if (t->state != KTHREAD_SUSPENDED) {
    sched_unlock();
    return -EINVAL;
  }

  kthread_list_add(t);

  // if ((current != NULL) && (kthread_priority_cmp(t, current)) > 0) {
  //   kthread_list_add(current);
  //   sched_yield();
  // }

  sched_unlock();

  return 0;
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
kthread_wakeup_all(struct ListLink *wait_queue)
{
  struct ListLink *l;
  struct KThread *t;

  if (!sched_locked())
    panic("scheduler not locked");

  while (!list_empty(wait_queue)) {
    l = wait_queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct KThread, link);
    kthread_list_add(t);
  }
}
