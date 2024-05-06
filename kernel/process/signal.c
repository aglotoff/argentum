#include <errno.h>
#include <signal.h>
#include <sys/mman.h>

#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/thread.h>
#include <kernel/cprintf.h>

static struct Signal *signal_alloc(int, int, int);
static void           signal_free(struct Signal *);
static int            signal_default_action(struct Process *, struct Signal *);
static int            signal_arch_prepare(struct Process *, struct Signal *);
static int            signal_arch_return(struct Process *);
static struct Signal *signal_dequeue(struct Process *);
static int            signal_valid_no(int, int);
static int            signal_valid_mask(const sigset_t *);

static struct ObjectPool *signal_cache;

void
signal_init_system(void)
{
  signal_cache = object_pool_create("signal_cache", sizeof(struct Signal), 0, NULL, NULL);
  if (signal_cache == NULL)
    panic("cannot allocate signal_cache");
}

/**
 * Initialize the signal-handling state of the process.
 * 
 * @param process The process
 */
void
signal_init(struct Process *process)
{
  int i;

  list_init(&process->signal_queue);
  process->signal_stub = 0;
  process->signal_mask = 0;
  process->signal_pending = 0;

  for (i = 0; i < NSIG; i++)
    process->signal_actions[i].sa_handler = SIG_DFL;  
}

void
signal_clone(struct Process *parent, struct Process *child)
{
  int i;

  if (!spin_holding(&__process_lock))
    panic("process_lock not acquired");

  for (i = 0; i < NSIG; i++)
    child->signal_actions[i] = parent->signal_actions[i];

  child->signal_stub = parent->signal_stub;
  child->signal_mask = parent->signal_mask;
}

int
signal_generate(pid_t pid, int signo, int code)
{
  struct ListLink *l;

  process_lock();

  LIST_FOREACH(&__process_list, l) {
    struct Process *process = LIST_CONTAINER(l, struct Process, link);
    struct sigaction *action = &process->signal_actions[signo - 1];
    struct Signal *signal;

    if (!process_match_pid(process, pid))
      continue;

    // Do not queue subsequent occurences of the same signal
    // TODO: any exceptions when queuing is required?
    if (sigismember(&process->signal_pending, signo))
      continue;

    // Blocked signals remain pending unless the action is to ignore the signal
    if (sigismember(&process->signal_mask, signo) &&
        (action->sa_handler == SIG_IGN))
      continue;

    if ((signal = signal_alloc(signo, code, 0)) == NULL) {
      process_unlock();
      return -ENOMEM;
    }

    // Notify the thread that a signal is pending
    list_add_back(&process->signal_queue, &signal->link);

    if (!sigismember(&process->signal_mask, signo))
      thread_interrupt(process->thread);
  }

  process_unlock();

  return 0;
}

void
signal_deliver_pending(void)
{
  struct Process *process = process_current();
  struct Signal *signal;
  struct sigaction *action;
  int exit_code;

  process_lock();

  if ((signal = signal_dequeue(process)) == NULL) {
    process_unlock();
    return;
  }

  action = &process->signal_actions[signal->info.si_signo - 1];

  if (action->sa_handler == SIG_DFL) {
    exit_code = signal_default_action(process, signal);
  } else if ((exit_code = signal_arch_prepare(process, signal)) == 0) {
    process->signal_mask |= action->sa_mask;
  }

  signal_free(signal);

  process_unlock();

  if (exit_code != 0)
    process_destroy(exit_code);
}

int
signal_return(void)
{
  struct Process *current = process_current();

  process_lock();

  if (signal_arch_return(current) != 0) {
    process_unlock();
    process_destroy(SIGKILL);
  }

  process_unlock();
  
  return current->thread->tf->r0;
}

int
signal_action(int no, uintptr_t stub, struct sigaction *action,
              struct sigaction *old_action)
{
  struct Process *current = process_current();

  if (!signal_valid_no(no, action && (action->_u._sa_handler != SIG_DFL)))
    return -EINVAL;

  if (stub && vm_space_check_ptr(current->vm, stub, PROT_READ | PROT_EXEC))
    return -EFAULT;

  process_lock();

  if (old_action != NULL) 
    *old_action = current->signal_actions[no - 1];

  if (action != NULL) {
    if ((action->_u._sa_handler != SIG_IGN) &&
        (action->_u._sa_handler != SIG_DFL) &&
        vm_space_check_ptr(current->vm, (uintptr_t) action->_u._sa_handler,
                           PROT_READ | PROT_EXEC)) {
      process_unlock();
      return -EFAULT;
    }

    if (!signal_valid_mask(&action->sa_mask)) {
      process_unlock();
      return -EINVAL;
    }

    current->signal_actions[no - 1] = *action;
  }

  if (stub)
    current->signal_stub = stub;

  process_unlock();
  
  return 0;
}

int
signal_pending(sigset_t *set)
{
  struct Process *process = process_current();

  process_lock();
  *set = process->signal_pending;
  process_unlock();

  return 0;
}

int
signal_mask(int how, const sigset_t *set, sigset_t *oldset)
{
  struct Process *process = process_current();

  if ((set != NULL) && !signal_valid_mask(set))
    return -EINVAL;
  
  if ((how != SIG_SETMASK) && (how != SIG_BLOCK) && (how != SIG_UNBLOCK))
    return -EINVAL;

  process_lock();

  if (oldset != NULL)
    *oldset = process->signal_mask;

  if (set != NULL) {
    switch (how) {
    case SIG_SETMASK:
      process->signal_mask = *set;
      break;
    case SIG_BLOCK:
      process->signal_mask |= *set;
      break;
    case SIG_UNBLOCK:
      process->signal_mask &= ~*set;
      break;
    }
  }
  
  process_unlock();

  return 0;
}

int
signal_suspend(const sigset_t *sigmask)
{
  sigset_t newmask, oldmask;
  int r;
  
  if (sigmask != NULL) {
    newmask = *sigmask;
    sigdelset(&newmask, SIGKILL);
    sigdelset(&newmask, SIGSTOP);
    signal_mask(SIG_BLOCK, &newmask, &oldmask);
  }

  sched_lock();
  r = sched_sleep(NULL, THREAD_STATE_SLEEPING_INTERRUPTILE, 0, NULL);
  sched_unlock();

  if (sigmask != NULL)
    signal_mask(SIG_SETMASK, &oldmask, NULL);

  return r;
}

static struct Signal *
signal_alloc(int signo, int code, int value)
{
  struct Signal *signal;

  if ((signal = (struct Signal *) object_pool_get(signal_cache)) == NULL)
    return NULL;
  
  signal->link.next = NULL;
  signal->link.prev = NULL;
  signal->info.si_signo = signo;
  signal->info.si_code = code;
  signal->info.si_value.sival_int = value;

  return signal;
}

static void
signal_free(struct Signal *signal)
{
  object_pool_put(signal_cache, signal);
}

static int
signal_arch_prepare(struct Process *process, struct Signal *signal)
{
  struct SignalFrame *ctx;

  ctx = ((struct SignalFrame *) process->thread->tf->sp) - 1;

  if ((vm_space_check_buf(process->vm, ctx, sizeof(*ctx), PROT_WRITE)) != 0)
    return SIGKILL;

  ctx->r0     = process->thread->tf->r0;
  ctx->sp     = process->thread->tf->sp;
  ctx->lr     = process->thread->tf->lr;
  ctx->pc     = process->thread->tf->pc;
  ctx->psr    = process->thread->tf->psr;
  ctx->mask   = process->signal_mask;
  
  ctx->signo = signal->info.si_signo;
  ctx->handler = (uintptr_t) (process->signal_actions[signal->info.si_signo - 1].sa_handler);

  process->thread->tf->r0 = (uint32_t) &ctx->signo;
  process->thread->tf->sp = (uintptr_t) ctx;
  process->thread->tf->pc = process->signal_stub;

  return 0;
}

// Restore the context saved prior to signal handling.
// A malicious user can mess with the context contents so we need to make
// some checks before proceeding.
static int
signal_arch_return(struct Process *process)
{
  struct SignalFrame *ctx;

  ctx = (struct SignalFrame *) process->thread->tf->sp;
  if ((vm_space_check_buf(process->vm, ctx, sizeof(*ctx), PROT_WRITE)) != 0)
    return -EFAULT;

  // Check the signal mask
  if (!signal_valid_mask(&ctx->mask))
    return -EINVAL;

  // Make sure we're returning to user mode
  if ((ctx->psr & PSR_M_MASK) != PSR_M_USR)
    return -EINVAL;

  // No need to check SP, LR, and PC - bad values will lead to page faults

  process->signal_mask = ctx->mask;

  process->thread->tf->r0  = ctx->r0;
  process->thread->tf->sp  = ctx->sp;
  process->thread->tf->lr  = ctx->lr;
  process->thread->tf->pc  = ctx->pc;
  process->thread->tf->psr = ctx->psr;

  return 0;
}

static int
signal_default_action(struct Process *current, struct Signal *signal)
{
  (void) current;
  
  switch (signal->info.si_signo) {
  case SIGABRT:
  case SIGBUS:
  case SIGFPE:
  case SIGILL:
  case SIGQUIT:
  case SIGSEGV:
  case SIGSYS:
  case SIGTRAP:
  case SIGXCPU:
  case SIGXFSZ:
    // TODO: abnormal termination with additional actions
    // fall through

  case SIGALRM:
  case SIGHUP:
  case SIGINT:
  case SIGKILL:
  case SIGPIPE:
  case SIGTERM:
  case SIGUSR1:
  case SIGUSR2:
    // Abnormal termination
    return signal->info.si_signo;

  case SIGCHLD:
  case SIGURG:
    // Ignore
    break;

  case SIGSTOP:
  case SIGTSTP:
  case SIGTTIN:
  case SIGTTOU:
    // Stop
    panic("not implemented %d", signal->info.si_signo);
    break;

  case SIGCONT:
    // Continue
    panic("not implemented %d", signal->info.si_signo);
    break;
  }

  return 0;
}

static struct Signal *
signal_dequeue(struct Process *process)
{
  struct ListLink *link;

  LIST_FOREACH(&process->signal_queue, link) {
    struct Signal *signal = LIST_CONTAINER(link, struct Signal, link);

    // Signal blocked: remain pending until unblocked or accepted
    if (sigismember(&process->signal_mask, signal->info.si_signo))
      continue;

    list_remove(&signal->link);
    return signal;
  }

  return NULL;
}

static int
signal_valid_mask(const sigset_t *mask)
{
  return !sigismember(mask, SIGKILL) && !sigismember(mask, SIGSTOP);
}

static int
signal_valid_no(int signo, int block)
{
  if ((signo < 0) || (signo >= NSIG))
    return 0;
  if (block && ((signo == SIGKILL) || (signo == SIGSTOP)))
    return 0;
  return 1;
}
