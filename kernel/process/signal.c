#include <errno.h>
#include <signal.h>
#include <sys/mman.h>

#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/thread.h>
#include <kernel/console.h>

#include <kernel/armv7/regs.h>

static struct Signal *signal_create(int, int, int);
static void           signal_free(struct Signal *);
static int            signal_default_action(struct Process *, struct Signal *);
static struct Signal *signal_dequeue(struct Process *);

static int            signal_arch_prepare(struct Process *, struct Signal *);
static int            signal_arch_return(struct Process *);

static struct KObjectPool *signal_cache;

// Initialize the signal-handling system
void
signal_init_system(void)
{
  signal_cache = k_object_pool_create("signal_cache",
                                      sizeof(struct Signal),
                                      0,
                                      NULL,
                                      NULL);
  if (signal_cache == NULL)
    panic("cannot allocate signal_cache");
}

// Initialize the signal-handling state of a single process.
void
signal_init(struct Process *process)
{
  int i;

  // process->signal_queue initialized in process_ctor
  process->signal_stub = 0;
  sigemptyset(&process->signal_mask);

  for (i = 0; i < NSIG; i++) {
    process->signal_actions[i].sa_handler = SIG_DFL;  
    process->signal_pending[i] = NULL;
  }
}

// TODO: reorder parent and child?
void
signal_clone(struct Process *parent, struct Process *child)
{
  int i;

  if (!k_spinlock_holding(&__process_lock))
    panic("process_lock not acquired");

  // process->signal_queue initialized in process_ctor
  child->signal_stub = parent->signal_stub;
  child->signal_mask = parent->signal_mask;

  for (i = 0; i < NSIG; i++) {
    child->signal_actions[i] = parent->signal_actions[i];
    child->signal_pending[i] = NULL;
  }
}

#define SIGNAL_INDEX(signo) (signo - 1)

static int
signal_is_stop(int signo)
{
  return (signo == SIGSTOP) ||
         (signo == SIGTSTP) ||
         (signo == SIGTTIN) ||
         (signo == SIGTTOU);
}

static int
signal_is_ignored(struct Process *process, int signo)
{
  struct sigaction *action = &process->signal_actions[SIGNAL_INDEX(signo)];
  
  if (action->sa_handler == SIG_IGN)
    return 1;

  if (action->sa_handler == SIG_DFL)
    return (signo == SIGCHLD) || (signo == SIGURG);

  return 0;
}

static int
signal_can_be_ignored(int signo)
{
  return (signo != SIGKILL) && (signo != SIGSTOP);
}

static void
signal_discard(struct Process *process, int signo)
{
  struct Signal *s = process->signal_pending[SIGNAL_INDEX(signo)];
  if (s != NULL) {
    process->signal_pending[SIGNAL_INDEX(signo)] = NULL;
    k_list_remove(&s->link);
  }
}

static int
signal_is_blocked(struct Process *process, int signo)
{
  if (!signal_can_be_ignored(signo))
    return 0;
  return sigismember(&process->signal_mask, signo);
}

static int signal_generate_one(struct Process *, int, int);

static void
signal_parent(struct Process *process)
{
  struct Process *parent = process->parent;

  // TODO: can parent be NULL or itself??
  if ((parent != NULL) && (parent != process)) {
    struct sigaction *act = &parent->signal_actions[SIGNAL_INDEX(SIGCHLD)];

    if (!(act->sa_flags & SA_NOCLDSTOP)) 
      signal_generate_one(parent, SIGCHLD, 0);
  }
}

static int
signal_generate_one(struct Process *process, int signo, int code)
{
  struct Signal *signal;

  if (!k_spinlock_holding(&__process_lock))
    panic("process_lock not acquired");

  // Do not queue subsequent occurences of the same signal
  // TODO: are there any exceptions when queuing is required?
  if (process->signal_pending[SIGNAL_INDEX(signo)])
    return 0;

  // If a stop signal is generated, discard all pending continue signals (and
  // vice versa)
  if (signal_is_stop(signo)) {
    signal_discard(process, SIGCONT);
  } else if (signo == SIGCONT) {
    signal_discard(process, SIGSTOP);
    signal_discard(process, SIGTSTP);
    signal_discard(process, SIGTTIN);
    signal_discard(process, SIGTTOU);

    // Continue stopped process even if SIGCONT is ignored or blocked
    if (process->state == PROCESS_STATE_STOPPED) {
      process->state = PROCESS_STATE_ACTIVE;
      k_thread_interrupt(process->thread);
      signal_parent(process);
    }
  }

  if (signal_is_ignored(process, signo))
    return 0;

  if ((signal = signal_create(signo, code, 0)) == NULL)
    return -ENOMEM;

  k_list_add_back(&process->signal_queue, &signal->link);
  process->signal_pending[SIGNAL_INDEX(signo)] = signal;

  // Blocked signals remain pending
  if (!signal_is_blocked(process, signo))
    k_thread_interrupt(process->thread);

  return 0;
}

int
signal_generate(pid_t pid, int signo, int code)
{
  struct KListLink *l;
  int r = 0;

  process_lock();

  KLIST_FOREACH(&__process_list, l) {
    struct Process *process = KLIST_CONTAINER(l, struct Process, link);

    if (!process_match_pid(process, pid))
      continue;

    if ((r = signal_generate_one(process, signo, code)) != 0)
      break;
  }

  process_unlock();

  return r;
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
  } else if (action->sa_handler == SIG_IGN) {
    panic("ignored signals should not be delivered");
  } else if ((exit_code = signal_arch_prepare(process, signal)) == 0) {
    process->signal_mask |= action->sa_mask;
    if (action->sa_flags & SA_RESETHAND)
      action->sa_handler = SIG_DFL;
  }

  signal_free(signal);

  process_unlock();

  if (exit_code != 0) {
    process_destroy(exit_code);
  }
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
signal_action(int signo,
              uintptr_t stub,
              struct sigaction *action,
              struct sigaction *old_action)
{
  struct Process *current = process_current();

  if ((signo < 0) || (signo >= NSIG))
    return -EINVAL;

  // SIGKILL and SIGSTOP cannot be ignored or catched
  if ((action != NULL) && (action->_u._sa_handler != SIG_DFL) &&
      !signal_can_be_ignored(signo))
    return -EINVAL;

  process_lock();

  if (old_action != NULL) 
    *old_action = current->signal_actions[signo - 1];

  if (action != NULL) {
    // TODO: SA_RESTART
    // TODO: SA_SIGINFO

    current->signal_actions[signo - 1] = *action;

    // Setting a signal action to SIG_IGN or setting it to SIG_DFL if the
    // default action is to ignore shall cause a pending signal to be discarded
    if (signal_is_ignored(current, signo))
      signal_discard(current, signo);
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
  int i;

  sigemptyset(set);

  process_lock();
  
  for (i = 1; i <= NSIG; i++)
    if (process->signal_pending[i - 1] != NULL)
      sigaddset(set, i);

  process_unlock();

  return 0;
}

int
signal_mask(int how, const sigset_t *set, sigset_t *oldset)
{
  struct Process *process = process_current();

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
  struct KWaitQueue wait_chan;
  sigset_t newmask, oldmask;
  int r;
  
  if (sigmask != NULL) {
    newmask = *sigmask;
    signal_mask(SIG_BLOCK, &newmask, &oldmask);
  }

  process_lock();

  k_waitqueue_init(&wait_chan);
  r = k_waitqueue_sleep(&wait_chan, &__process_lock);

  process_unlock();

  if (sigmask != NULL)
    signal_mask(SIG_SETMASK, &oldmask, NULL);

  return r;
}

static struct Signal *
signal_create(int signo, int code, int value)
{
  struct Signal *signal;

  if ((signal = (struct Signal *) k_object_pool_get(signal_cache)) == NULL)
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
  k_object_pool_put(signal_cache, signal);
}

static int
signal_default_action(struct Process *current, struct Signal *signal)
{ 
  int signo = signal->info.si_signo;

  switch (signo) {
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
    if ((current->parent != NULL) && (current->parent != current))
      signal_generate_one(current->parent, SIGCHLD, 0);
    return signo;

  case SIGCHLD:
  case SIGURG:
    panic("ignored signals should not be delivered");
    break;

  case SIGTSTP:
  case SIGTTIN:
  case SIGTTOU:
    // TODO: A process that is a member of an orphaned process group shall not
    // be allowed to stop in response to the SIGTSTP, SIGTTIN, or SIGTTOU
    // fall through
  case SIGSTOP:
    // Stop
    current->state = PROCESS_STATE_STOPPED;
    current->status = 0x7f;
    // current->flags |= PROCESS_STATUS_AVAILABLE;
    signal_parent(current);
    break;

  case SIGCONT:
    // Continue
    if (current->state == PROCESS_STATE_STOPPED)
      panic("The process must be already continued");
    break;
  }

  return 0;
}

static struct Signal *
signal_dequeue(struct Process *process)
{
  struct KListLink *link;

  KLIST_FOREACH(&process->signal_queue, link) {
    struct Signal *signal = KLIST_CONTAINER(link, struct Signal, link);
    int signo = signal->info.si_signo;

    // Blocked signals remain pending until either unblocked or accepted
    if (signal_is_blocked(process, signo))
      continue;

    // If stopped, all signals except SIGKILL shall not be delivered until the
    // process is continued
    if ((process->state == PROCESS_STATE_STOPPED) &&
        (signo != SIGKILL) && (signo != SIGCONT))
      continue;

    k_list_remove(&signal->link);
    process->signal_pending[SIGNAL_INDEX(signo)] = NULL;

    return signal;
  }

  return NULL;
}

struct SignalFrame {
  // Saved by user:
  uint32_t s[32];
  uint32_t fpscr;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;

  // Saved by kernel:
  uint32_t signo;
  uint32_t handler;
  uint32_t r0;
  uint32_t sp;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
  uint32_t trapno;
  sigset_t mask;
};

static int
signal_arch_prepare(struct Process *process, struct Signal *signal)
{
  uintptr_t ctx_va = process->thread->tf->sp - sizeof(struct SignalFrame);
  int signo = signal->info.si_signo;
  struct SignalFrame ctx;

  ctx.r0      = process->thread->tf->r0;
  ctx.sp      = process->thread->tf->sp;
  ctx.lr      = process->thread->tf->lr;
  ctx.pc      = process->thread->tf->pc;
  ctx.psr     = process->thread->tf->psr;
  ctx.mask    = process->signal_mask;
  ctx.signo   = signo;
  ctx.handler = (uintptr_t) (process->signal_actions[signo - 1].sa_handler);

  if (vm_copy_out(process->vm->pgtab, &ctx, ctx_va, sizeof ctx) != 0)
    return SIGKILL;

  process->thread->tf->r0 = ctx_va + offsetof(struct SignalFrame, signo);
  process->thread->tf->sp = ctx_va;
  process->thread->tf->pc = process->signal_stub;

  return 0;
}

// Restore the context saved prior to signal handling.
// A malicious user can mess with the context contents so we need to make
// some checks before proceeding.
static int
signal_arch_return(struct Process *process)
{
  uintptr_t ctx_va = process->thread->tf->sp;
  struct SignalFrame ctx;

  if (vm_copy_in(process->vm->pgtab, &ctx, ctx_va, sizeof ctx) != 0)
    return -EFAULT;

  // Make sure we're returning to user mode
  if ((ctx.psr & PSR_M_MASK) != PSR_M_USR)
    panic("bad PSR");

  // No need to check SP, LR, and PC - bad values will lead to page faults

  process->signal_mask     = ctx.mask;
  process->thread->tf->r0  = ctx.r0;
  process->thread->tf->sp  = ctx.sp;
  process->thread->tf->lr  = ctx.lr;
  process->thread->tf->pc  = ctx.pc;
  process->thread->tf->psr = ctx.psr;

  return 0;
}
