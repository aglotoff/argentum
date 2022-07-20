#include <assert.h>
#include <string.h>

#include <armv7.h>
#include <cprintf.h>
#include <cpu.h>
#include <list.h>
#include <mm/kobject.h>
#include <mm/vm.h>
#include <process.h>
#include <sync.h>
#include <scheduler.h>

static struct KObjectPool *task_pool;

// Run queue
static struct {
  struct ListLink run_queue;
  struct SpinLock lock;
} sched;

static void scheduler_yield(void);

void context_switch(struct Context **, struct Context *);

void
scheduler_init(void)
{
  task_pool = kobject_pool_create("task_pool", sizeof(struct Task), 0);
  if (task_pool == NULL)
    panic("cannot allocate task pool");

  list_init(&sched.run_queue);
  spin_init(&sched.lock, "sched");
}

void
scheduler_start(void)
{
  struct ListLink *link;
  struct Task *next;

  for (;;) {
    irq_enable();

    spin_lock(&sched.lock);

    while (!list_empty(&sched.run_queue)) {
      link = sched.run_queue.next;
      list_remove(link);

      next = LIST_CONTAINER(link, struct Task, link);
      assert(next->state == TASK_RUNNABLE);

      next->state = TASK_RUNNING;
      my_cpu()->task = next;

      if (next->process != NULL)
        vm_switch_user(next->process->vm);

      context_switch(&my_cpu()->scheduler, next->context);

      if (next->process != NULL)
        vm_switch_kernel();
    }

    // Mark that no process is running on this CPU.
    my_cpu()->task = NULL;

    spin_unlock(&sched.lock);

    wfi();
  }
}

static void
scheduler_yield(void)
{
  int irq_flags;

  irq_flags = my_cpu()->irq_flags;
  context_switch(&my_task()->context, my_cpu()->scheduler);
  my_cpu()->irq_flags = irq_flags;
}

struct Task *
task_create(struct Process *process, void (*entry)(void), uint8_t *stack)
{
  struct Task *task;

  if ((task = (struct Task *) kobject_alloc(task_pool)) == NULL)
    return NULL;

  stack -= sizeof *task->context;
  task->context = (struct Context *) stack;
  memset(task->context, 0, sizeof *task->context);
  task->context->lr = (uint32_t) task_run;
  task->entry = entry;

  task->process = process;

  return task;
}

void
task_destroy(struct Task *task)
{
  if (task == my_task())
    panic("a task cannot destroy itself");

  kobject_free(task_pool, task);
}

void
task_yield(void)
{
  struct Task *current = my_task();
  
  spin_lock(&sched.lock);

  current->state = TASK_RUNNABLE;
  list_add_back(&sched.run_queue, &current->link);

  // Return into the scheduler loop
  scheduler_yield();

  spin_unlock(&sched.lock);
}

// A process' very first scheduling by scheduler() will switch here.
void
task_run(void)
{
  // Still holding the process table lock.
  spin_unlock(&sched.lock);

  my_task()->entry();
}

/**
 * 
 */
void
task_sleep(struct ListLink *wait_queue, struct SpinLock *lock)
{
  struct Task *current = my_task();

  if (lock != &sched.lock) {
    spin_lock(&sched.lock);
    spin_unlock(lock);
  }

  list_add_back(wait_queue, &current->link);

  current->state = TASK_NOT_RUNNABLE;

  scheduler_yield();

  if (lock != &sched.lock) {
    spin_unlock(&sched.lock);
    spin_lock(lock);
  }
}

void
task_enqueue(struct Task *th)
{
  spin_lock(&sched.lock);

  th->state = TASK_RUNNABLE;
  list_add_back(&sched.run_queue, &th->link);

  spin_unlock(&sched.lock);
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param wait_queue Pointer to the head of the wait queue.
 */
void
task_wakeup(struct ListLink *wait_queue)
{
  struct ListLink *l;
  struct Task *t;

  spin_lock(&sched.lock);

  while (!list_empty(wait_queue)) {
    l = wait_queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct Task, link);

    t->state = TASK_RUNNABLE;
    list_add_back(&sched.run_queue, &t->link);
  }

  spin_unlock(&sched.lock);
}
