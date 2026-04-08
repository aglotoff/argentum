#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

#include <kernel/core/semaphore.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/core/task.h>
#include <kernel/console.h>

#include "process_private.h"

static struct Signal *signal_create(int, int, uintptr_t);
static void           signal_free(struct Signal *);
static int            signal_action_default(struct Thread *, struct Signal *, struct sigaction *);
static int            signal_action_custom(struct Thread *, struct Signal *, struct sigaction *);
static struct Signal *signal_dequeue(struct Thread *);
static int            signal_generate_one(struct Thread *, int, int);
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
    k_panic("cannot allocate signal_cache");
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
  k_assert(k_list_is_null(&signal->link));
}

static void
signal_init_thread(struct Thread *thread)
{
  sigemptyset(&thread->signal_current_mask);
  sigemptyset(&thread->signal_saved_mask);

  for (int i = 0; i < NSIG; i++)
    thread->signal_pending[i] = NULL;
}

// Initialize the signal-handling state of a single process.
void
signal_init(struct Process *process)
{
  struct KListLink *tl;

  // process->signal_queue initialized in process_ctor
  process->signal_stub = 0;

  for (int i = 0; i < NSIG; i++)
    process->signal_actions[i].sa_handler = SIG_DFL;  

  // TODO: foreach thread
  k_assert(!k_list_is_empty(&process->threads));

  K_LIST_FOREACH(&process->threads, tl) {
    struct Thread *thread = K_CONTAINER_OF(tl, struct Thread, process_link);
    signal_init_thread(thread);
  }
}

static void
signal_reset_thread(struct Thread *thread)
{
  sigemptyset(&thread->signal_current_mask);
  sigemptyset(&thread->signal_saved_mask);

  for (int i = 0; i < NSIG; i++)
    thread->signal_pending[i] = NULL;

  while (!k_list_is_empty(&thread->signal_queue)) {
    struct Signal *signal = K_CONTAINER_OF(thread->signal_queue.next, struct Signal, link);

    k_list_remove(&signal->link);
    signal_free(signal);
  }
}

void
signal_reset(struct Process *process)
{
  struct KListLink *tl;

  k_assert(k_spinlock_holding(&__process_lock));

  for (int i = 0; i < NSIG; i++)
    process->signal_actions[i].sa_handler = SIG_DFL;  

  // TODO: foreach thread
  k_assert(!k_list_is_empty(&process->threads));

  K_LIST_FOREACH(&process->threads, tl) {
    struct Thread *thread = K_CONTAINER_OF(tl, struct Thread, process_link);
    signal_reset_thread(thread);
  }
}

static void
signal_clone_thread(struct Thread *parent, struct Thread *child)
{
  child->signal_current_mask = parent->signal_current_mask;
  child->signal_saved_mask = parent->signal_saved_mask;

  for (int i = 0; i < NSIG; i++)
    child->signal_pending[i] = NULL;
}

// TODO: reorder parent and child?
void
signal_clone(struct Process *parent, struct Process *child)
{
  if (!k_spinlock_holding(&__process_lock))
    k_panic("process_lock not acquired");

  child->signal_stub = parent->signal_stub;
  
  for (int i = 0; i < NSIG; i++)
    child->signal_actions[i] = parent->signal_actions[i];

  signal_clone_thread(K_CONTAINER_OF(parent->threads.next, struct Thread, process_link),
                      K_CONTAINER_OF(child->threads.next, struct Thread, process_link));
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
signal_discard(struct Thread *thread, int signo)
{
  struct Signal *s = thread->signal_pending[SIGNAL_INDEX(signo)];
  if (s != NULL) {
    thread->signal_pending[SIGNAL_INDEX(signo)] = NULL;
    k_list_remove(&s->link);
  }
}

static int
signal_is_blocked(struct Thread *thread, int signo)
{
  if (!signal_can_be_ignored(signo))
    return 0;
  return sigismember(&thread->signal_current_mask, signo);
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
    if (signal_is_ignored(current, signo)) {
      // TODO: foreach thread
      struct KListLink *tl;

      K_LIST_FOREACH(&current->threads, tl) {
        struct Thread *thread = K_CONTAINER_OF(tl, struct Thread, process_link);
        signal_discard(thread, signo);
      }
    }
  }

  if (stub)
    current->signal_stub = stub;

  process_unlock();
  
  return 0;
}

int
signal_pending(sigset_t *set)
{
  struct Thread *thread = thread_current();
  int i;

  if (set == NULL)
    k_panic("set is NULL");

  sigemptyset(set);

  process_lock();

  k_assert(thread != NULL);
  
  for (i = 1; i <= NSIG; i++)
    if (thread->signal_pending[SIGNAL_INDEX(i)] != NULL)
      sigaddset(set, i);

  process_unlock();

  return 0;
}

int
signal_mask_change(int how, const sigset_t *set, sigset_t *old_set)
{
  struct Thread *thread = thread_current();
  int r = 0, i;

  k_assert(thread != NULL);

  if (old_set != NULL)
    *old_set = thread->signal_current_mask;

  if (set == NULL)
    return r;

  process_lock();

  switch (how) {
  case SIG_SETMASK:
    thread->signal_current_mask = *set;
    sigdelset(&thread->signal_current_mask, SIGKILL);
    break;

  case SIG_BLOCK:
    for (i = 1; i <= NSIG; i++)
      if ((i != SIGKILL) && sigismember(set, i))
        sigaddset(&thread->signal_current_mask, i);
    break;

  case SIG_UNBLOCK:
    for (i = 1; i <= NSIG; i++)
      if (sigismember(set, i))
        sigdelset(&thread->signal_current_mask, i);
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
  struct Thread *thread = thread_current();
  int r;

  if (mask == NULL)
    k_panic("mask is NULL");

  process_lock();

  k_assert(thread != NULL);

  thread->signal_saved_mask = thread->signal_current_mask;

  thread->signal_current_mask = *mask;
  sigdelset(&thread->signal_current_mask, SIGKILL);

  process_unlock();

  r = k_semaphore_get(&thread->sem, K_SLEEP_WAKEABLE);

  process_lock();

  thread->signal_current_mask = thread->signal_saved_mask;

  process_unlock();

  return r;
}

void
_signal_state_change_to_parent(struct Process *process)
{
  struct Process *parent = process->parent;
  struct KListLink *tl;

  k_assert(!k_spinlock_holding(&__process_lock));
  k_assert(k_mutex_holding(&__process_sleep_lock));

  // TODO: can parent be NULL or itself??
  if ((parent == NULL) || (parent == process))
    return;

  process_lock();

  if (process->state != PROCESS_STATE_ZOMBIE) {
    struct sigaction *sa = &parent->signal_actions[SIGNAL_INDEX(SIGCHLD)];

    if ((sa != NULL) && (sa->sa_flags & SA_NOCLDSTOP)) {
      process_unlock();
      return;
    }
  }

  // TODO: each?
  K_LIST_FOREACH(&parent->threads, tl) {
    struct Thread *thread = K_CONTAINER_OF(tl, struct Thread, process_link);
    signal_generate_one(thread, SIGCHLD, 0);
  }

  process_unlock();

  k_condvar_notify_all(&parent->wait_queue);
}

int
signal_return(void)
{
  struct Thread *current = thread_current();
  struct SignalFrame frame;
  int r, ret;

  process_lock();

  k_assert(current != NULL);

  if ((r = arch_signal_return(current, &frame, &ret)) != 0)
    return r;

  current->signal_current_mask = frame.ucontext.uc_sigmask;
  sigdelset(&current->signal_current_mask, SIGKILL);

  process_unlock();

  return ret;
}

int
signal_generate(pid_t pid, int signo, int code)
{
  struct KListLink *l;
  int r = 0;

  process_sleep_lock();

  K_LIST_FOREACH(&__process_list, l) {
    struct Process *process = K_CONTAINER_OF(l, struct Process, link);
    struct KListLink *tl;

    if (!process_match_pid(process, pid))
      continue;

    if (signo == 0)
      continue;
      
    // Continue stopped process even if SIGCONT is ignored or blocked
    if (signo == SIGCONT)
      _process_continue(process);

    process_lock();

    K_LIST_FOREACH(&process->threads, tl) {
      struct Thread *thread = K_CONTAINER_OF(tl, struct Thread, process_link);

      if ((r = signal_generate_one(thread, signo, code)) != 0)
        break;
    }

    process_unlock();
  }

  process_sleep_unlock();

  return r;
}

static int
signal_generate_one(struct Thread *thread, int signo, int code)
{
  struct Signal *signal;

  k_assert((signo > 0) && (signo <= NSIG));
  k_assert(k_spinlock_holding(&__process_lock));

  // TODO: check for permissions

  // Do not queue subsequent occurences of the same signal
  if (thread->signal_pending[SIGNAL_INDEX(signo)])
    return 0;

  // If a stop signal is generated, discard all pending continue signals (and
  // vice versa)
  if (signal_is_stop(signo)) {
    signal_discard(thread, SIGCONT);
  } else if (signo == SIGCONT) {
    signal_discard(thread, SIGSTOP);
    signal_discard(thread, SIGTSTP);
    signal_discard(thread, SIGTTIN);
    signal_discard(thread, SIGTTOU);
  }

  if (signal_is_ignored(thread->process, signo))
    return 0;

  if ((signal = signal_create(signo, code, 0)) == NULL)
    return -ENOMEM;

  k_list_add_back(&thread->signal_queue, &signal->link);
  thread->signal_pending[SIGNAL_INDEX(signo)] = signal;

  // Blocked signals remain pending
  // TODO: move out of the locked code!
  if (!signal_is_blocked(thread, signo)) {
    k_semaphore_set(&thread->sem, 1);
    k_task_wake(&thread->task);
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

int
signal_deliver_pending(void)
{
  struct Thread *thread = thread_current();
  struct Signal *signal;
  struct sigaction *action;
  int signo, r;
  int exit_code = 0;

  process_lock();

  k_assert(thread->process != NULL);

  if ((signal = signal_dequeue(thread)) == NULL) {
    r = thread->process->state != PROCESS_STATE_ACTIVE;
    process_unlock();
    return r;
  }

  signo = signal->info.si_signo;
  action = &thread->process->signal_actions[SIGNAL_INDEX(signo)];

  if (action->sa_handler == SIG_DFL) {
    process_unlock();
    exit_code = signal_action_default(thread, signal, action);
    process_lock();
  } else if (action->sa_handler == SIG_IGN) {
    k_panic("ignored signals should not be delivered");
  } else {
    exit_code = signal_action_custom(thread, signal, action);
  }

  process_unlock();

  signal_free(signal);

  if (exit_code != 0)
    process_destroy(exit_code);

  process_lock();
  r = thread->process->state != PROCESS_STATE_ACTIVE;
  process_unlock();

  return r;
}

static struct Signal *
signal_dequeue(struct Thread *thread)
{
  struct KListLink *link;

  K_LIST_FOREACH(&thread->signal_queue, link) {
    struct Signal *signal = K_CONTAINER_OF(link, struct Signal, link);
    int signo = signal->info.si_signo;

    // Blocked signals remain pending until either unblocked or accepted
    if (signal_is_blocked(thread, signo))
      continue;

    k_assert(thread->process != NULL);

    // If stopped, all signals except SIGKILL shall not be delivered until the
    // process is continued
    if ((thread->process->state == PROCESS_STATE_STOPPED) &&
        (signo != SIGKILL) && (signo != SIGCONT))
      continue;

    k_list_remove(&signal->link);
    thread->signal_pending[SIGNAL_INDEX(signo)] = NULL;

    return signal;
  }

  return NULL;
}

static int
signal_action_default(struct Thread *current,
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
    k_panic("ignored signals should not be delivered");
    break;

  case SIGTSTP:
  case SIGTTIN:
  case SIGTTOU:
    // TODO: A process that is a member of an orphaned process group shall not
    // be allowed to stop in response to the SIGTSTP, SIGTTIN, or SIGTTOU
    // fall through
  case SIGSTOP:
    process_sleep_lock();
    _process_stop(current->process, signo);
    process_sleep_unlock();
    break;

  case SIGCONT:
    // Continue (already continued)
    k_assert(current->process->state != PROCESS_STATE_STOPPED);
    break;
  }

  return 0;
}

static int
signal_action_custom(struct Thread *thread,
                     struct Signal *signal,
                     struct sigaction *sa)
{
  struct SignalFrame frame;

  memset(&frame, sizeof frame, 0);

  frame.info = signal->info;
  frame.handler = (uintptr_t) sa->sa_handler;
  frame.ucontext.uc_sigmask = thread->signal_current_mask;

  if (arch_signal_prepare(thread, &frame) != 0)
    return SIGKILL;
    
  thread->signal_current_mask |= sa->sa_mask;

  if (sa->sa_flags & SA_RESETHAND)
    sa->sa_handler = SIG_DFL;
  
  return 0;
}

static void
signal_free(struct Signal *signal)
{
  k_object_pool_put(signal_cache, signal);
}
