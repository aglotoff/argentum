#include <assert.h>
#include <errno.h>

#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/object.h>
#include <kernel/page.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>

static struct SpinLock thread_lock = SPIN_INITIALIZER("thread");

static struct ListLink thread_queues[THREAD_PRIORITY_MAX + 1];

static struct ObjectCache *thread_cache;

static int thread_next_id = 0;

#define THREAD_ID_HASH_SIZE 64

static struct ListLink thread_id_hash[THREAD_ID_HASH_SIZE];

static struct Thread *thread_dequeue(void);
static void           thread_enqueue(struct Thread *);

void
thread_init(void)
{
  int i;

  for (i = 0; i <= THREAD_PRIORITY_MAX; i++)
    list_init(&thread_queues[i]);

  for (i = 0; i < THREAD_ID_HASH_SIZE; i++)
    list_init(&thread_id_hash[i]);

  thread_cache = object_cache_create("thread", sizeof(struct Thread), 0,
                                     NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread_cache");
}

void
thread_start(void)
{
  struct Cpu *my_cpu;

  spin_lock(&thread_lock);

  my_cpu = smp_cpu();

  for (;;) {
    struct Thread *next = thread_dequeue();

    if (next != NULL) {
      assert(next->state == THREAD_STATE_READY);

      next->state = THREAD_STATE_RUNNING;

      next->cpu = my_cpu;
      my_cpu->thread = next;

      arch_thread_switch(&my_cpu->sched_context, next->context);

      // next->state is already modified elsewhere
      // the thread is already put into the queue if necessary

      my_cpu->thread = NULL;
      next->cpu = NULL;
    } else {
      spin_unlock(&thread_lock);

      irq_enable();
      arch_thread_idle();

      spin_lock(&thread_lock);
    }
  }
}

int
thread_create(void (*func)(void *), void *func_arg, int priority)
{
  struct Thread *thread;
  struct Page *stack_page;
  int id, err;
  
  if ((priority < 0) || (priority > THREAD_PRIORITY_MAX)) {
    err = EINVAL;
    goto fail1;
  }

  if ((thread = (struct Thread *) object_alloc(thread_cache)) == NULL) {
    err = -ENOMEM;
    goto fail1;
  }

  if ((stack_page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
    err = -ENOMEM;
    goto fail2;
  }

  thread->kstack = page2kva(stack_page);
  stack_page->ref_count++;

  thread->cpu      = NULL;
  thread->flags    = 0;
  thread->func     = func;
  thread->func_arg = func_arg;
  thread->priority = priority;
  thread->state    = THREAD_STATE_NONE;

  arch_thread_create(thread);

  spin_lock(&thread_lock);

  id = ++thread_next_id;

  // TODO: reuse old IDs
  if (id == 0)
    panic("too many threads");

  thread->id = id;
  list_add_back(&thread_id_hash[id % THREAD_ID_HASH_SIZE], &thread->id_link);

  thread_enqueue(thread);
  
  spin_unlock(&thread_lock);

  return 0;

  fail2:
    object_free(thread_cache, thread);
  fail1:
    return err;
}

// Execution of each thread begins here.
void
thread_run(void)
{
  struct Thread *my_thread = smp_cpu()->thread;

  // Still holding the scheduler lock (acquired in sched_start)
  spin_unlock(&thread_lock);

  // Make sure IRQs are enabled
  irq_enable();

  // Jump to the task entry point
  my_thread->func(my_thread->func_arg);

  // Destroy the task on exit
  thread_exit(0);
}

void
thread_exit(int status)
{
  struct Thread *my_thread;

  spin_lock(&thread_lock);

  my_thread = smp_cpu()->thread;
  kprintf("Thread %p exiting with code %d\n", my_thread->func_arg, status);

  arch_thread_switch(&my_thread->context, smp_cpu()->sched_context);

  panic("should not return");
}

void
thread_tick(void)
{
  struct Thread *my_thread;
  
  spin_lock(&thread_lock);

  my_thread = smp_cpu()->thread;
  if (my_thread != NULL)
    my_thread->flags |= THREAD_FLAGS_YIELD;

  spin_unlock(&thread_lock);
}

void
thread_may_yield(void)
{
  struct Thread *my_thread;
  
  spin_lock(&thread_lock);

  my_thread = smp_cpu()->thread;

  if ((my_thread != NULL) && (my_thread->flags & THREAD_FLAGS_YIELD)) {
    my_thread->flags &= ~THREAD_FLAGS_YIELD;

    thread_enqueue(my_thread);
    arch_thread_switch(&my_thread->context, smp_cpu()->sched_context);
  }

  spin_unlock(&thread_lock);
}

// static struct Thread *
// thread_get(int thread_id)
// {
//   struct ListLink *l;

//   LIST_FOREACH(&thread_queues[thread_id % THREAD_ID_HASH_SIZE], l) {
//     struct Thread *thread = LIST_CONTAINER(l, struct Thread, link);
//     if (thread->id == thread_id)
//       return thread;
//   }

//   return NULL;
// }

static struct Thread *
thread_dequeue(void)
{
  struct ListLink *link;
  int i;

  assert(spin_holding(&thread_lock));

  for (i = 0; i <= THREAD_PRIORITY_MAX; i++) {
    if (!list_empty(&thread_queues[i])) {
      link = thread_queues[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct Thread, link);
    }
  }

  return NULL;
}

static void
thread_enqueue(struct Thread *thread)
{
  assert(spin_holding(&thread_lock));
  
  thread->state = THREAD_STATE_READY;
  list_add_back(&thread_queues[thread->priority], &thread->link);
}
