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

static struct {
  struct ListLink queue[TASK_MAX_PRIORITIES];
  struct SpinLock lock;
} sched;

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
    list_init(&sched.queue[i]);

  spin_init(&sched.lock, "sched");
}

// Add the specified task to the run queue with the corresponding priority
static void
sched_enqueue(struct Task *th)
{
  assert(spin_holding(&sched.lock));

  th->state = TASK_STATE_READY;
  list_add_back(&sched.queue[th->priority], &th->link);
}

// Retrieve the highest-priority task from the run queue
static struct Task *
sched_dequeue(void)
{
  struct ListLink *link;
  int i;
  
  assert(spin_holding(&sched.lock));

  for (i = 0; i < TASK_MAX_PRIORITIES; i++) {
    if (!list_empty(&sched.queue[i])) {
      link = sched.queue[i].next;
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

  spin_lock(&sched.lock);

  my_cpu = cpu_current();

  for (;;) {
    struct Task *next = sched_dequeue();

    if (next != NULL) {
      struct TaskHooks *hooks = next->hooks;

      assert(next->state == TASK_STATE_READY);

      if ((hooks != NULL) && (hooks->prepare_switch != NULL))
        hooks->prepare_switch(next);

      next->state = TASK_STATE_RUNNING;
      my_cpu->task = next;

      context_switch(&my_cpu->scheduler, next->context);

      my_cpu->task = NULL;

      if ((hooks != NULL) && (hooks->finish_switch != NULL))
        hooks->finish_switch(next);

      // Perform cleanup for the exited task
      if (next->state == TASK_STATE_ZOMBIE) {
        next->state = TASK_STATE_UNUSED;

        if ((hooks != NULL) && (hooks->destroy != NULL)) {
          spin_unlock(&sched.lock);
          hooks->destroy(next);
          spin_lock(&sched.lock);
        }
      }
    } else {
      spin_unlock(&sched.lock);

      cpu_irq_enable();
      asm volatile("wfi");

      spin_lock(&sched.lock);
    }
  }
}

// Switch back from the current task context back to the scheduler loop
static void
sched_yield(void)
{
  int irq_flags;

  assert(spin_holding(&sched.lock));

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

  spin_lock(&sched.lock);

  my_cpu = cpu_current();

  if (my_cpu->isr_nesting <= 0)
    panic("isr_nesting <= 0");

  if (--my_cpu->isr_nesting == 0) {
    struct Task *my_task = my_cpu->task;

    if (my_task != NULL) {
      // Before resuming the current task, check whether it must give up the
      // CPU due to a higher-priority task becoming available or due to time
      // quanta exhaustion.
      if (my_task->flags & TASK_FLAGS_RESCHEDULE) {
        my_task->flags &= ~TASK_FLAGS_RESCHEDULE;

        sched_enqueue(my_task);
        sched_yield();
      }

      // TODO: the task could also be marked for desruction?
    }
  }

  spin_unlock(&sched.lock);
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
    spin_lock(&sched.lock);
    current_task->flags |= TASK_FLAGS_RESCHEDULE;
    spin_unlock(&sched.lock);
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
task_init(struct Task *task, void (*entry)(void), int priority, uint8_t *stack,
          struct TaskHooks *hooks)
{
  task->flags = 0;
  task->priority = priority;
  task->state = TASK_STATE_SUSPENDED;
  task->entry = entry;
  task->hooks = hooks;

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

  spin_lock(&sched.lock);

  // TODO: state-specific cleanup

  // Cleanup for the current task will be performed from the scheduler context
  if (task == my_task) {
    task->state = TASK_STATE_ZOMBIE;
    sched_yield();
    panic("should not return");
  }

  if (task->state == TASK_STATE_RUNNING) {
    // TODO: the task is executing on another CPU! send IPI in this case
    panic("not implmented");
  }

  task->state = TASK_STATE_UNUSED;

  hooks = task->hooks;

  spin_unlock(&sched.lock);

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

  spin_lock(&sched.lock);

  sched_enqueue(current);
  sched_yield();

  spin_unlock(&sched.lock);
}

// Execution of each task begins here.
static void
task_run(void)
{
  // Still holding the scheduler lock (acquired in sched_start)
  spin_unlock(&sched.lock);

  // Make sure IRQs are enabled
  cpu_irq_enable();

  // Jump to the task entry point
  task_current()->entry();
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
task_check_resched(struct Task *recent)
{
  struct Cpu *my_cpu;
  struct Task *my_task;

  assert(spin_holding(&sched.lock));

  my_cpu  = cpu_current();
  my_task = my_cpu->task;

  if ((my_task != NULL) && (task_priority_cmp(recent, my_task) > 0)) {
    if (my_cpu->isr_nesting > 0) {
      // Cannot yield inside an ISR handler, delay until the last call to
      // sched_isr_exit()
      my_task->flags |= TASK_FLAGS_RESCHEDULE;
    } else {
      sched_enqueue(my_task);
      sched_yield();
    }
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
  spin_lock(&sched.lock);

  if (task->state != TASK_STATE_SUSPENDED) {
    spin_unlock(&sched.lock);
    return -EINVAL;
  }

  sched_enqueue(task);
  task_check_resched(task);

  spin_unlock(&sched.lock);

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

  // someone may call this function while holding sched.lock?
  if (lock != &sched.lock) {
    spin_lock(&sched.lock);
    if (lock != NULL)
      spin_unlock(lock);
  }

  assert(spin_holding(&sched.lock));

  if (queue != NULL)
    list_add_back(queue, &current->link);

  current->state = state;
  sched_yield();

  // someone may call this function while holding sched.lock?
  if (lock != &sched.lock) {
    spin_unlock(&sched.lock);
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

  spin_lock(&sched.lock);

  LIST_FOREACH(queue, l) {
    struct Task *t = LIST_CONTAINER(l, struct Task, link);
    
    if ((highest == NULL) || (task_priority_cmp(t, highest) > 0))
      highest = t;
  }

  if (highest != NULL) {
    list_remove(&highest->link);
    sched_enqueue(highest);

    task_check_resched(highest);
  }

  spin_unlock(&sched.lock);
}

/**
 * Wake up all processes sleeping on the wait queue.
 *
 * @param queue Pointer to the head of the wait queue.
 */
void
task_wakeup_all(struct ListLink *queue)
{
  spin_lock(&sched.lock);

  while (!list_empty(queue)) {
    struct ListLink *l;
    struct Task *t;

    l = queue->next;
    list_remove(l);

    t = LIST_CONTAINER(l, struct Task, link);
    sched_enqueue(t);

    task_check_resched(t);
  }

  spin_unlock(&sched.lock);
}
