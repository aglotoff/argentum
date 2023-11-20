#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/elf.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/object.h>
#include <kernel/page.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static struct SpinLock thread_lock = SPIN_INITIALIZER("thread");

static struct ListLink thread_queues[THREAD_PRIORITY_MAX + 1];

static struct ObjectCache *thread_cache;

static struct Thread  *thread_dequeue(void);
static void            thread_enqueue(struct Thread *);
// static struct Process *process_get(pid_t);

void
thread_init(void)
{
  int i;

  for (i = 0; i <= THREAD_PRIORITY_MAX; i++)
    list_init(&thread_queues[i]);

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

      if (next->process && next->process->vm)
        arch_vm_load(next->process->vm);

      arch_thread_switch(&my_cpu->sched_context, next->context);

      if (next->process && next->process->vm)
        arch_vm_load(kernel_pgtab);

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

static struct Thread *
thread_alloc(struct Process *process, void (*func)(void *), void *func_arg,
             int priority)
{
  struct Thread *thread;
  struct Page *stack_page;
  
  if ((priority < 0) || (priority > THREAD_PRIORITY_MAX))
    return NULL;

  if ((thread = (struct Thread *) object_alloc(thread_cache)) == NULL)
    return NULL;

  if ((stack_page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
    object_free(thread_cache, thread);
    return NULL;
  }

  stack_page->ref_count++;

  thread->kstack      = page2kva(stack_page);
  thread->kstack_size = PAGE_SIZE;
  thread->tf          = NULL;
  thread->cpu         = NULL;
  thread->flags       = 0;
  thread->func        = func;
  thread->func_arg    = func_arg;
  thread->priority    = priority;
  thread->state       = THREAD_STATE_NONE;
  thread->process     = process;

  return thread;
}

static void
thread_start_user(void *arg)
{
  struct Thread *current = thread_current();

  (void) arg;
  
  // Allocate user-space stack for the current thread
  if ((vm_range_alloc(current->process->vm, VIRT_USTACK_TOP - PAGE_SIZE,
                      PAGE_SIZE, VM_READ | VM_WRITE | VM_USER)) < 0)
    thread_exit(1);

  arch_thread_pop_tf(thread_current());
}

struct Thread *
thread_create_user(struct Process *process, uintptr_t entry)
{
  struct Thread *thread;

  if ((thread = thread_alloc(process, thread_start_user, NULL, 1)) == NULL)
    return NULL;

  arch_thread_create_user(thread, (void *) entry, NULL, NULL, NULL);

  spin_lock(&thread_lock);

  process->active_threads++;
  list_add_back(&process->threads, &thread->process_link);

  thread_enqueue(thread);
  spin_unlock(&thread_lock);

  return thread;
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
  struct Thread *current = thread_current();

  kprintf("Thread %p exiting with code %d\n", current->func_arg, status);

  spin_lock(&thread_lock);

  current->process->active_threads--;
  current->state = THREAD_STATE_DESTROYED;

  if (current->process->active_threads == 0) {
    // TODO: if this is the last thread, notify a wait()'ing a parent
    kprintf("Last thread is destroyed\n");
  }

  // TODO: else if someone wants to join() this thread, notify it
  // TODO: destroy process resources

  arch_thread_switch(&current->context, smp_cpu()->sched_context);

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

struct Thread *
thread_current(void)
{
  struct Thread *current;

  irq_save();
  current = smp_cpu()->thread;
  irq_restore();

  return current;
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

// static struct Process *
// process_get(pid_t id)
// {
//   struct ListLink *l;

//   LIST_FOREACH(&process_id_hash[id % PROCESS_ID_HASH_SIZE], l) {
//     struct Process *process = LIST_CONTAINER(l, struct Process, id_link);
//     if (process->id == id)
//       return process;
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
