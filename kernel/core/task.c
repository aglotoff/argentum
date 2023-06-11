#include <assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/task.h>
#include <kernel/spinlock.h>

static void task_run(void);
void context_switch(struct Context **, struct Context *);

static struct ListLink sched_queue[TASK_MAX_PRIORITIES];
struct SpinLock __sched_lock = SPIN_INITIALIZER("sched");

/**
 * Initialize the scheduler data structures.
 * 
 * This function must be called prior to creating any kernel tasks.
 */
void
sched_init(void)
{
  int i;

  for (i = 0; i < TASK_MAX_PRIORITIES; i++)
    list_init(&sched_queue[i]);
}

// Add the specified task to the run queue with the corresponding priority
static void
sched_enqueue(struct Task *th)
{
  assert(spin_holding(&__sched_lock));

  th->state = TASK_STATE_READY;
  list_add_back(&sched_queue[th->priority], &th->link);
}

// Retrieve the highest-priority task from the run queue
static struct Task *
sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  assert(spin_holding(&__sched_lock));

  for (i = 0; i < TASK_MAX_PRIORITIES; i++) {
    if (!list_empty(&sched_queue[i])) {
      link = sched_queue[i].next;
      list_remove(link);

      return LIST_CONTAINER(link, struct Task, link);
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
    struct Task *next = sched_dequeue();

    if (next != NULL) {
      struct TaskHooks *hooks = next->hooks;

      assert(next->state == TASK_STATE_READY);

      if ((hooks != NULL) && (hooks->prepare_switch != NULL))
        hooks->prepare_switch(next);

      next->state = TASK_STATE_RUNNING;

      next->u.running.cpu = my_cpu;
      my_cpu->task = next;

      context_switch(&my_cpu->scheduler, next->context);

      my_cpu->task = NULL;

      if ((hooks != NULL) && (hooks->finish_switch != NULL))
        hooks->finish_switch(next);

      // Perform cleanup for the exited task
      if ((next->state == TASK_STATE_DESTROYED) && (next->destroyer == NULL)) {
        next->state = TASK_STATE_NONE;

        if ((hooks != NULL) && (hooks->destroy != NULL)) {
          sched_unlock();
          hooks->destroy(next);
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

// Switch back from the current task context back to the scheduler loop
static void
sched_yield(void)
{
  int irq_flags;

  assert(spin_holding(&__sched_lock));

  irq_flags = cpu_current()->irq_flags;
  context_switch(&task_current()->context, cpu_current()->scheduler);
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
    struct Task *my_task = my_cpu->task;

    // Before resuming the current task, check whether it must give up the CPU
    // or exit.
    if (my_task != NULL) {
      if ((my_task->flags & TASK_FLAGS_DESTROY) &&
          (my_task->protect_count == 0)) {
        task_cleanup(my_task);

        if (my_task->destroyer != NULL)
          sched_enqueue(my_task->destroyer);

        sched_yield();
        panic("should not return");
      }

      if ((my_task->flags & TASK_FLAGS_RESCHEDULE) &&
          (my_task->lock_count == 0)) {
        my_task->flags &= ~TASK_FLAGS_RESCHEDULE;

        sched_enqueue(my_task);
        sched_yield();
      }
    }
  }

  sched_unlock();
}

/**
 * Notify the kernel that a timer IRQ has occured.
 */
void
sched_tick(void)
{
  struct Task *current_task = task_current();

  // Tell the scheduler that the current task has used up its time slice
  // TODO: add support for more complex sheculing policies
  if (current_task != NULL) {
    sched_lock();
    current_task->flags |= TASK_FLAGS_RESCHEDULE;
    sched_unlock();
  }
}

/**
 * Get the currently executing task.
 * 
 * @return A pointer to the currently executing task or NULL
 */
struct Task *
task_current(void)
{
  struct Task *task;

  cpu_irq_save();
  task = cpu_current()->task;
  cpu_irq_restore();

  return task;
}

/**
 * Initialize the kernel task. After successful initialization, the task
 * is put into suspended state and must be explicitly made runnable by a call
 * to task_resume().
 * 
 * @param process  Pointer to a process the task belongs to.
 * @param task   Pointer to the kernel task to be initialized.
 * @param entry    task entry point function.
 * @param priority task priority value.
 * @param stack    Top of the task stack.
 * 
 * @return 0 on success.
 */
int
task_create(struct Task *task, void (*entry)(void), int priority, uint8_t *stack,
          struct TaskHooks *hooks)
{
  task->flags         = 0;
  task->priority      = priority;
  task->state         = TASK_STATE_SUSPENDED;
  task->entry         = entry;
  task->hooks         = hooks;
  task->destroyer     = NULL;
  task->lock_count    = 0;
  task->protect_count = 0;

  stack -= sizeof *task->context;
  task->context = (struct Context *) stack;
  memset(task->context, 0, sizeof *task->context);
  task->context->lr = (uint32_t) task_run;

  return 0;
}

/**
 * Destroy the specified task
 */
void
task_destroy(struct Task *task)
{
  struct Task *my_task = task_current();
  struct TaskHooks *hooks;

  if (task == NULL)
    task = my_task;

  sched_lock();

  if ((task == NULL) || (task->flags & TASK_FLAGS_DESTROY)) {
    // TODO: report an error if task is NULL
    sched_unlock();
    return;
  }

  if (task == my_task) {
    // TODO: what if protect_lock > 0?

    task_cleanup(task);

    sched_yield();
    panic("should not return");
  }

  if ((task->state == TASK_STATE_RUNNING) || (task->protect_count > 0)) {
    task->flags |= TASK_FLAGS_DESTROY;

    if (task->protect_count == 0)
      irq_ipi();

    if (my_task == NULL) {
      sched_unlock();
      return;
    }

    task->destroyer = my_task;

    my_task->state = TASK_STATE_DESTROY;
    my_task->u.destroy.task = task;

    sched_yield();
  } else {
    task_cleanup(task);
  }

  task->state = TASK_STATE_NONE;

  hooks = task->hooks;

  sched_unlock();

  // Execute the "destroy" hook in the context of the current task
  if ((hooks != NULL) && (hooks->destroy != NULL))
    hooks->destroy(task);
}

/**
 * Relinguish the CPU allowing another task to be run.
 */
void
task_yield(void)
{
  struct Task *current = task_current();
  
  if (current == NULL)
    panic("no current task");

  sched_lock();

  sched_enqueue(current);
  sched_yield();

  sched_unlock();
}

// Execution of each task begins here.
static void
task_run(void)
{
  // Still holding the scheduler lock (acquired in sched_start)
  sched_unlock();

  // Make sure IRQs are enabled
  cpu_irq_enable();

  // Jump to the task entry point
  task_current()->entry();

  // Destroy the task on exit
  task_destroy(NULL);
}

// Compare task priorities. Note that a smaller priority value corresponds
// to a higher priority! Returns a number less than, equal to, or greater than
// zero if t1's priority is correspondingly less than, equal to, or greater than
// t2's priority.
static int
task_priority_cmp(struct Task *t1, struct Task *t2)
{
  return t2->priority - t1->priority; 
}

// Check whether a reschedule is required (taking into account the priority
// of a task most recently added to the run queue)
static void
sched_may_yield(struct Task *candidate)
{
  struct Cpu *my_cpu;
  struct Task *my_task;

  assert(spin_holding(&__sched_lock));

  my_cpu  = cpu_current();
  my_task = my_cpu->task;

  if ((my_task != NULL) && (task_priority_cmp(candidate, my_task) > 0)) {
    if ((my_cpu->isr_nesting > 0) || (my_task->lock_count > 0)) {
      // Cannot yield right now, delay until the last call to sched_isr_exit()
      // or task_unlock().
      my_task->flags |= TASK_FLAGS_RESCHEDULE;
    } else {
      sched_enqueue(my_task);
      sched_yield();
    }
  }
}

void
sched_wakeup_all(struct ListLink *task_list)
{
  assert(spin_holding(&__sched_lock));
  
  while (!list_empty(task_list)) {
    struct ListLink *link = task_list->next;
    struct Task *task = LIST_CONTAINER(link, struct Task, link);

    list_remove(link);

    sched_enqueue(task);
    sched_may_yield(task);
  }
}

/**
 * Resume execution of a previously suspended task (or begin execution of a
 * newly created one).
 * 
 * @param task The kernel task to resume execution
 */
int
task_resume(struct Task *task)
{
  sched_lock();

  if (task->state != TASK_STATE_SUSPENDED) {
    sched_unlock();
    return -EINVAL;
  }

  sched_enqueue(task);
  sched_may_yield(task);

  sched_unlock();

  return 0;
}



/**
 * Put the current task into sleep.
 *
 * @param queue An optional queue to insert the task into.
 * @param state The state indicating a kind of sleep.
 * @param lock  An optional spinlock to release while going to sleep.
 */
void
task_sleep(struct ListLink *queue, int state, struct SpinLock *lock)
{
  struct Task *current = task_current();

  // someone may call this function while holding __sched_lock?
  if (lock != &__sched_lock) {
    sched_lock();
    if (lock != NULL)
      spin_unlock(lock);
  }

  assert(spin_holding(&__sched_lock));

  current->state = state;

  if (queue != NULL)
    list_add_back(queue, &current->link);

  sched_yield();

  // someone may call this function while holding __sched_lock?
  if (lock != &__sched_lock) {
    sched_unlock();
    if (lock != NULL)
      spin_lock(lock);
  }
}

/**
 * Wake up the task with the highest priority.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
task_wakeup_one(struct ListLink *queue)
{
  struct ListLink *l;
  struct Task *highest;

  highest = NULL;

  sched_lock();

  LIST_FOREACH(queue, l) {
    struct Task *t = LIST_CONTAINER(l, struct Task, link);
    
    if ((highest == NULL) || (task_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    list_remove(&highest->link);
    sched_enqueue(highest);

    sched_may_yield(highest);
  }

  sched_unlock();
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
task_wakeup_all(struct ListLink *queue)
{
  sched_lock();
  sched_wakeup_all(queue);
  sched_unlock();
}

void
task_lock(struct Task *task)
{
  if ((task == NULL) && ((task = task_current()) == NULL))
    panic("no current task");

  sched_lock();
  task->lock_count++;
  sched_unlock();
}

void
task_unlock(struct Task *task)
{
  struct Task *my_task = task_current();

  if ((task == NULL) && ((task = my_task) == NULL))
    panic("no current task");

  sched_lock();

  if ((--task->lock_count == 0) && (task->flags & TASK_FLAGS_RESCHEDULE)) {
    if (task == my_task) {
      if (cpu_current()->isr_nesting == 0) {
        sched_enqueue(task);
        sched_yield();
      }
    } else if (task->state == TASK_STATE_RUNNING) {
      irq_ipi();
    }
  }

  sched_unlock();
}

void
task_protect(struct Task *task)
{
  if ((task == NULL) && ((task = task_current()) == NULL))
    panic("no current task");

  sched_lock();
  task->protect_count++;
  sched_unlock();
}

void
task_cleanup(struct Task *task)
{
  switch (task->state) {
  case TASK_STATE_DESTROY:
    task->u.destroy.task->destroyer = NULL;
    break;
  // TODO: other states
  default:
    break;
  }

  task->state = TASK_STATE_DESTROYED;
}

int
task_unprotect(struct Task *task)
{
  struct Task *my_task = task_current();

  if ((task == NULL) && ((task = my_task) == NULL))
    return -EINVAL;

  sched_lock();

  if ((--task->protect_count > 0) || !(task->flags & TASK_FLAGS_DESTROY)) {
    // Nothing to do
    sched_unlock();
    return 0;
  }

  task_cleanup(task);

  if (task->destroyer != NULL)
    sched_enqueue(task->destroyer);

  if (task == my_task) {
    sched_yield();
    panic("should not return");
  } else if (task->destroyer != NULL) {
    sched_may_yield(task->destroyer);
  }

  sched_unlock();

  return 0;
}
