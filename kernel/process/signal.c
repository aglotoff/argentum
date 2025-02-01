#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/thread.h>
#include <kernel/console.h>

#include "process_private.h"

static struct Signal *signal_create(int, int, uintptr_t);
static void           signal_free(struct Signal *);
static int            signal_action_default(struct Process *, struct Signal *, struct sigaction *);
static int            signal_action_custom(struct Process *, struct Signal *, struct sigaction *);
static struct Signal *signal_dequeue(struct Process *);
static int            signal_generate_one(struct Process *, int, int);
static void           signal_ctor(void *, size_t);
static void           signal_dtor(void *, size_t);

static struct KObjectPool *signal_cache;

// Initialize the signal-handling system
void
signal_init_system(void)
{
  signal_cache = k_object_pool_create("signal_cache",
                                      sizeof(struct Signal),
                                      0,
                                      signal_ctor,
                                      signal_dtor);
  if (signal_cache == NULL)
    panic("cannot allocate signal_cache");
}

static void
signal_ctor(void *p, size_t)
{
  struct Signal *signal = (struct Signal *) p;
  k_list_null(&signal->link);
}

static void
signal_dtor(void *p, size_t)
{
  struct Signal *signal = (struct Signal *) p;
  assert(k_list_is_null(&signal->link));
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

int
signal_action_change(int signo,
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
    *old_action = current->signal_actions[SIGNAL_INDEX(signo)];

  if (action != NULL) {
    // TODO: SA_RESTART
    // TODO: SA_SIGINFO

    current->signal_actions[SIGNAL_INDEX(signo)] = *action;

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

  if (set == NULL)
    panic("set is NULL");

  sigemptyset(set);

  process_lock();
  
  for (i = 1; i <= NSIG; i++)
    if (process->signal_pending[SIGNAL_INDEX(i)] != NULL)
      sigaddset(set, i);

  process_unlock();

  return 0;
}

int
signal_mask_change(int how, const sigset_t *set, sigset_t *old_set)
{
  struct Process *process = process_current();
  int r = 0, i;

  if (old_set != NULL)
    *old_set = process->signal_mask;

  if (set == NULL)
    return r;

  process_lock();

  switch (how) {
  case SIG_SETMASK:
    process->signal_mask = *set;
    sigdelset(&process->signal_mask, SIGKILL);
    break;

  case SIG_BLOCK:
    for (i = 1; i <= NSIG; i++)
      if ((i != SIGKILL) && sigismember(set, i))
        sigaddset(&process->signal_mask, i);
    break;

  case SIG_UNBLOCK:
    for (i = 1; i <= NSIG; i++)
      if (sigismember(set, i))
        sigdelset(&process->signal_mask, i);
    break;

  default:
    r = -EINVAL;
    break;
  }
  
  process_unlock();

  return r;
}

int
signal_suspend(const sigset_t *mask)
{
  struct Process *process = process_current();
  struct KWaitQueue wait_chan;
  sigset_t saved_mask;
  int r;

  if (mask == NULL)
    panic("mask is NULL");

  process_lock();

  saved_mask = process->signal_mask;

  process->signal_mask = *mask;
  sigdelset(&process->signal_mask, SIGKILL);

  k_waitqueue_init(&wait_chan);
  r = k_waitqueue_sleep(&wait_chan, &__process_lock);

  process->signal_mask = saved_mask;

  process_unlock();

  return r;
}

void
_signal_state_change_to_parent(struct Process *process)
{
  struct Process *parent = process->parent;

  assert(k_spinlock_holding(&__process_lock));

  // TODO: can parent be NULL or itself??
  if ((parent == NULL) || (parent == process))
    return;

  if (process->state != PROCESS_STATE_ZOMBIE) {
    struct sigaction *sa = &parent->signal_actions[SIGNAL_INDEX(SIGCHLD)];

    if ((sa != NULL) && (sa->sa_flags & SA_NOCLDSTOP))
      return;
  }

  signal_generate_one(parent, SIGCHLD, 0);

  k_waitqueue_wakeup_all(&parent->wait_queue);
}

int
signal_return(void)
{
  struct Process *current = process_current();
  struct SignalFrame frame;
  int r, ret;

  process_lock();

  if ((r = arch_signal_return(current, &frame, &ret)) != 0)
    return r;

  current->signal_mask = frame.ucontext.uc_sigmask;
  sigdelset(&current->signal_mask, SIGKILL);

  process_unlock();

  return ret;
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

    if (signo == 0)
      continue;

    if ((r = signal_generate_one(process, signo, code)) != 0)
      break;
  }

  process_unlock();

  return r;
}

static int
signal_generate_one(struct Process *process, int signo, int code)
{
  struct Signal *signal;

  assert((signo > 0) && (signo <= NSIG));
  assert(k_spinlock_holding(&__process_lock));

  // TODO: check for permissions

  // Do not queue subsequent occurences of the same signal
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
    _process_continue(process);
  }

  if (signal_is_ignored(process, signo))
    return 0;

  if ((signal = signal_create(signo, code, 0)) == NULL)
    return -ENOMEM;

  k_list_add_back(&process->signal_queue, &signal->link);
  process->signal_pending[SIGNAL_INDEX(signo)] = signal;

  // Blocked signals remain pending
  if (!signal_is_blocked(process, signo)) {
    k_thread_interrupt(process->thread);
  }

  return 0;
}

static struct Signal *
signal_create(int signo, int code, uintptr_t value)
{
  struct Signal *signal;

  if ((signal = (struct Signal *) k_object_pool_get(signal_cache)) == NULL)
    return NULL;
  
  signal->info.si_signo = signo;
  signal->info.si_code = code;
  signal->info.si_value.sival_ptr = (void *) value;

  return signal;
}

void
signal_deliver_pending(void)
{
  struct Process *process = process_current();
  struct Signal *signal;
  struct sigaction *sa;
  int exit_code = 0;

  process_lock();

  if ((signal = signal_dequeue(process)) == NULL) {
    process_unlock();
    return;
  }

  sa = &process->signal_actions[SIGNAL_INDEX(signal->info.si_signo)];

  if (sa->sa_handler == SIG_DFL) {
    exit_code = signal_action_default(process, signal, sa);
  } else if (sa->sa_handler == SIG_IGN) {
    panic("ignored signals should not be delivered");
  } else {
    if (process->pid == 200)
      cprintf("delivering signal %d %p\n", signal->info.si_signo, sa->sa_handler);
    exit_code = signal_action_custom(process, signal, sa);
  }

  signal_free(signal);

  process_unlock();

  if (exit_code != 0) {
    process_destroy(exit_code);
  }
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

static int
signal_action_default(struct Process *current,
                      struct Signal *signal,
                      struct sigaction *sa)
{ 
  int signo = signal->info.si_signo;

  (void) sa;

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
    _process_stop(current);
    break;

  case SIGCONT:
    // Continue
    if (current->state == PROCESS_STATE_STOPPED)
      panic("The process must be already continued");
    break;
  }

  return 0;
}

static int
signal_action_custom(struct Process *process,
                     struct Signal *signal,
                     struct sigaction *sa)
{
  struct SignalFrame frame;

  memset(&frame, sizeof frame, 0);

  frame.info = signal->info;
  frame.handler = (uintptr_t) sa->sa_handler;
  frame.ucontext.uc_sigmask = process->signal_mask;

  if (arch_signal_prepare(process, &frame) != 0)
    return SIGKILL;
    
  process->signal_mask |= sa->sa_mask;

  if (sa->sa_flags & SA_RESETHAND)
    sa->sa_handler = SIG_DFL;
  
  return 0;
}

static void
signal_free(struct Signal *signal)
{
  k_object_pool_put(signal_cache, signal);
}
