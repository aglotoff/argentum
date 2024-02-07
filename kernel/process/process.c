#include <kernel/assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <kernel/armv7/regs.h>
#include <kernel/cpu.h>
#include <kernel/cprintf.h>
#include <kernel/elf.h>
#include <kernel/fd.h>
#include <kernel/fs/fs.h>
#include <kernel/hash.h>
#include <kernel/object_pool.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/monitor.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>
#include <kernel/vmspace.h>
#include <kernel/trap.h>
#include <kernel/ktime.h>
#include <kernel/ksemaphore.h>

struct ObjectPool *process_cache;
struct ObjectPool *thread_cache;
struct ObjectPool *signal_cache;

// Size of PID hash table
#define NBUCKET   256

// Process ID hash table
static struct {
  struct ListLink table[NBUCKET];
  struct SpinLock lock;
} pid_hash;

// Lock to protect the parent/child relationships between the processes
static struct SpinLock process_lock;

static void process_run(void *);
static void arch_trap_frame_pop(struct TrapFrame *);

static struct Process *init_process;

static void
process_ctor(void *buf, size_t size)
{
  struct Process *proc = (struct Process *) buf;

  wchan_init(&proc->wait_queue);
  list_init(&proc->children);
  list_init(&proc->pending_signals);

  (void) size;
}

void
process_init(void)
{
  extern uint8_t _binary_obj_user_init_start[];

  process_cache = object_pool_create("process_cache", sizeof(struct Process), 0, 0, process_ctor, NULL);
  if (process_cache == NULL)
    panic("cannot allocate process_cache");
  
  signal_cache = object_pool_create("signal_cache", sizeof(struct Signal), 0, 0, NULL, NULL);
  if (signal_cache == NULL)
    panic("cannot allocate signal_cache");

  HASH_INIT(pid_hash.table);
  spin_init(&pid_hash.lock, "pid_hash");

  spin_init(&process_lock, "process_lock");

  // Create the init process
  if (process_create(_binary_obj_user_init_start, &init_process) != 0)
    panic("Cannot create the init process");
}

struct Process *
process_alloc(void)
{
  static pid_t next_pid;
  int i;
  struct Process *process;

  if ((process = (struct Process *) object_pool_get(process_cache)) == NULL)
    return NULL;

  process->thread = thread_create(process, process_run, process, NZERO);
  if (process->thread == NULL) {
    object_pool_put(process_cache, process);
    return NULL;
  }

  list_init(&process->pending_signals);
  for (i = 0; i < NSIG; i++)
    process->signal_handlers[i].sa_handler = SIG_DFL;

  process->parent = NULL;
  process->zombie = 0;
  process->sibling_link.next = NULL;
  process->sibling_link.prev = NULL;

  spin_lock(&pid_hash.lock);

  if ((process->pid = ++next_pid) < 0)
    panic("pid overflow");

  HASH_PUT(pid_hash.table, &process->pid_link, process->pid);

  spin_unlock(&pid_hash.lock);

  fd_init(process);

  return process;
}

int
process_setup_vm(struct Process *proc)
{
  if ((proc->vm = vm_space_create()) == NULL)
    return -ENOMEM;

  return 0;
}

void 
arch_trap_frame_init(struct Process *process, uintptr_t entry, uintptr_t arg1,
                   uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  process->thread->tf->r0  = arg1;              // argc
  process->thread->tf->r1  = arg2;              // argv
  process->thread->tf->r2  = arg3;              // environ
  process->thread->tf->sp  = sp;                // stack pointer
  process->thread->tf->psr = PSR_M_USR | PSR_F; // user mode, interrupts enabled
  process->thread->tf->pc  = entry;             // process entry point
}

static int
process_load_binary(struct Process *proc, const void *binary)
{
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;
  int r;

  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0)
    return -EINVAL;

  ph = (Elf32_Phdr *) ((uint8_t *) elf + elf->phoff);
  eph = ph + elf->phnum;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;

    if (ph->filesz > ph->memsz)
      return -EINVAL;

    if ((r = vm_range_alloc(proc->vm, ph->vaddr, ph->memsz,
                            VM_READ | VM_WRITE | VM_EXEC | VM_USER)) < 0)
      return r;

    if ((r = vm_space_copy_out(proc->vm, (void *) ph->vaddr,
                         (uint8_t *) elf + ph->offset, ph->filesz)) < 0)
      return r;

    proc->vm->heap = ROUND_UP(ph->vaddr + ph->memsz, PAGE_SIZE);
  }

  if ((r = vm_range_alloc(proc->vm, (VIRT_USTACK_TOP - USTACK_SIZE),
                          USTACK_SIZE, VM_READ | VM_WRITE | VM_USER)) < 0)
    return r;
  proc->vm->stack = VIRT_USTACK_TOP - USTACK_SIZE;

  arch_trap_frame_init(proc, elf->entry, 0, 0, 0, VIRT_USTACK_TOP);

  return 0;
}

int
process_create(const void *binary, struct Process **pstore)
{
  struct Process *proc;
  int r;

  if ((proc = process_alloc()) == NULL) {
    r = -ENOMEM;
    goto fail1;
  }

  if ((r = process_setup_vm(proc) < 0))
    goto fail2;

  if ((r = process_load_binary(proc, binary)) < 0)
    goto fail3;

  proc->pgid = 0;
  proc->ruid = proc->euid = 0;
  proc->rgid = proc->egid = 0;
  proc->cmask = 0;

  thread_resume(proc->thread);

  if (pstore != NULL)
    *pstore = proc;

  return 0;

fail3:
  vm_space_destroy(proc->vm);
fail2:
  process_free(proc);
fail1:
  return r;
}

/**
 * Free all resources associated with a process.
 * 
 * @param proc The process to be freed.
 */
void
process_free(struct Process *process)
{
  // Return the process descriptor to the cache
  object_pool_put(process_cache, process);
}

struct Process *
pid_lookup(pid_t pid)
{
  struct ListLink *l;
  struct Process *proc;

  spin_lock(&pid_hash.lock);

  HASH_FOREACH_ENTRY(pid_hash.table, l, pid) {
    proc = LIST_CONTAINER(l, struct Process, pid_link);
    if (proc->pid == pid) {
      spin_unlock(&pid_hash.lock);
      return proc;
    }
  }

  spin_unlock(&pid_hash.lock);
  return NULL;
}

void
process_destroy(int status)
{
  struct ListLink *l;
  struct Process *child, *current = process_current();
  int has_zombies;

  if(status)
    cprintf("[k] process #%d destroyed with code 0x%x\n", current->pid, status);

  // Remove the pid hash link
  // TODO: place this code somewhere else?
  spin_lock(&pid_hash.lock);
  HASH_REMOVE(&current->pid_link);
  spin_unlock(&pid_hash.lock);

  vm_space_destroy(current->vm);

  fd_close_all(current);
  fs_inode_put(current->cwd);

  assert(init_process != NULL);

  spin_lock(&process_lock);

  // Move children to the init process
  has_zombies = 0;
  while (!list_empty(&current->children)) {
    l = current->children.next;
    list_remove(l);

    child = LIST_CONTAINER(l, struct Process, sibling_link);
    child->parent = init_process;
    list_add_back(&init_process->children, l);

    // Check whether there is a child available to be cleaned up
    if (child->zombie)
      has_zombies = 1;
  }

  // Wake up the init process to cleanup zombie children
  if (has_zombies)
    wchan_wakeup_all(&init_process->wait_queue);

  current->zombie = 1;
  current->exit_code = status;

  // Wakeup the parent process
  if (current->parent)
    wchan_wakeup_all(&current->parent->wait_queue);

  spin_unlock(&process_lock);

  thread_exit();
}

pid_t
process_copy(int share_vm)
{
  struct Process *child, *current = process_current();
  int i;

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->vm = vm_space_clone(current->vm, share_vm)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->parent         = current;
  *child->thread->tf    = *current->thread->tf;
  child->thread->tf->r0 = 0;

  fd_clone(current, child);

  for (i = 0; i < NSIG; i++)
    child->signal_handlers[i] = current->signal_handlers[i];

  child->pgid  = current->pgid;
  child->ruid  = current->ruid;
  child->euid  = current->euid;
  child->rgid  = current->rgid;
  child->egid  = current->egid;
  child->cmask = current->cmask;
  child->cwd   = fs_inode_duplicate(current->cwd);

  spin_lock(&process_lock);
  list_add_back(&current->children, &child->sibling_link);
  spin_unlock(&process_lock);

  thread_resume(child->thread);

  return child->pid;
}

int
process_wait_check(struct Process *current, struct Process *process, pid_t pid)
{
  (void) current;

  if (pid == process->pid)
    return 1;
  if (pid == -1)
    return 1;
  if (pid == 0)
    // TODO: compare the child group ID to the parent group ID
    return 0;
  if (pid < 0)
    // TODO: compare the child group ID to (-pid)
    return 0;
  return 0;
}

pid_t
process_wait(pid_t pid, int *stat_loc, int options)
{
  struct Process *current = process_current();
  struct ListLink *l;
  int r, has_match;

  // cprintf("[k] process #%d waits for #%d\n", current->pid, pid);

  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  spin_lock(&process_lock);

  for (;;) {
    has_match = 0;

    LIST_FOREACH(&current->children, l) {
      struct Process *process = LIST_CONTAINER(l, struct Process, sibling_link);

      if (process_wait_check(current, process, pid))
        has_match = process->pid;

      // Return immediately
      if (process->zombie) {
        list_remove(&process->sibling_link);

        spin_unlock(&process_lock);

        if (stat_loc)
          *stat_loc = process->exit_code;

        process_free(process);

        return has_match;
      }
    }

    if (!has_match) {
      r = -ECHILD;
      break;
    }

    if (options & WNOHANG) {
      r = 0;
      break;
    }

    if ((r = wchan_sleep(&current->wait_queue, &process_lock)) != 0)
      break;
  }

  spin_unlock(&process_lock);

  return r;
}

static void
process_run(void *arg)
{
  static int first;

  struct Process *process = (struct Process *) arg;

  if (!first) {
    first = 1;

    fs_init();

    if ((process->cwd == NULL) && (fs_name_lookup("/", 0, &process->cwd) < 0))
      panic("root not found");
  }

  // "Return" to the user space.
  arch_trap_frame_pop(process->thread->tf);
}

// Load the user-mode registers from the trap frame.
// Doesn't return.
static void
arch_trap_frame_pop(struct TrapFrame *tf)
{
  asm volatile(
    "mov     sp, %0\n"
    "b       trap_user_exit\n"
    :
    : "r" (tf)
    : "memory"
  );
}

void *
process_grow(ptrdiff_t increment)
{
  struct Process *current = process_current();
  uintptr_t prev_heap = current->vm->heap;
  uintptr_t prev_limit = ROUND_UP(prev_heap, PAGE_SIZE);
  intptr_t r;

  if (increment == 0) {
    r = prev_heap;
  } else if (increment > 0) {
    uintptr_t next_heap = prev_heap + increment;

    if ((next_heap < prev_heap) || (next_heap > current->vm->stack)) {
      r = -ENOMEM;
    } else {
      uintptr_t next_limit = ROUND_UP(next_heap, PAGE_SIZE);

      if (next_limit != prev_limit) {
        if ((r = vm_range_alloc(current->vm, prev_limit, next_limit - prev_limit, VM_WRITE | VM_READ | VM_USER)) == 0) {
          current->vm->heap = next_heap;
          r = prev_heap;
        }
      } else {
        current->vm->heap = next_heap;
        r = prev_heap;
      }
    }
  } else if (increment < 0) {
    uintptr_t next_heap = prev_heap + increment;

    if ((next_heap > prev_heap) || (next_heap < PAGE_SIZE)) {
      r = -ENOMEM;
    } else {
      uintptr_t next_limit = ROUND_UP(next_heap, PAGE_SIZE);

      if (next_limit != prev_limit) {
        vm_range_free(current->vm, next_limit, prev_limit - next_limit);
        current->vm->heap = next_heap;
        r = prev_heap;
      } else {
        current->vm->heap = next_heap;
        r = prev_heap;
      }
    }
  }

  // cprintf("sbrk(%ld) -> %p, new = %p, %u pages left\n", increment, (void *) r, current->vm->heap, pages_free);
  return (void *) r;
}

struct Signal *
process_signal_create(int sig)
{
  struct Signal *signal;

  if ((signal = (struct Signal *) object_pool_get(signal_cache)) == NULL)
    panic("out of memory");
  
  signal->link.next = NULL;
  signal->link.prev = NULL;
  signal->info.si_signo = sig;
  signal->info.si_code = 0;
  signal->info.si_value.sival_int = 0;

  return signal;
}

void
process_signal_send(struct Process *process, struct Signal *signal)
{
  struct sigaction *handler;

  spin_lock(&process_lock);

  handler = &process->signal_handlers[signal->info.si_signo];
  if (handler->sa_handler == SIG_IGN) {
    spin_unlock(&process_lock);
    object_pool_put(signal_cache, signal);
    return;
  }

  list_add_back(&process->pending_signals, &signal->link);
  // TODO: interrupt waiting

  spin_unlock(&process_lock);
}

int
process_signal_action(int sig, uintptr_t stub, struct sigaction *act,
                      struct sigaction *oact)
{
  struct Process *current = process_current();
  struct sigaction *handler;

  if (stub >= VIRT_KERNEL_BASE)
    return -EFAULT;
  
  if (sig < 0 || sig >= NSIG)
    return -EINVAL;
  if (sig == SIGKILL || sig == SIGSTOP)
    return -EINVAL;
  
  if (stub != 0)
    current->signal_stub = stub;

  handler = &current->signal_handlers[sig];

  if (oact != NULL)
    *oact = *handler;

  if (act != NULL)
    *handler = *act;
  
  return 0;
}

void
process_signal_default(struct Process *current, struct Signal *signal)
{
  int terminate = 0;

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
    // TODO: abnormal terminatio with additional actions
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
    terminate = signal->info.si_signo;
    break;

  case SIGCHLD:
  case SIGURG:
    // Ignore
    break;

  case SIGSTOP:
  case SIGTSTP:
  case SIGTTIN:
  case SIGTTOU:
    // Stop
    panic("not implemented");
    break;

  case SIGCONT:
    // Continue
    panic("not implemented");
    break;
  }

  spin_unlock(&process_lock);

  object_pool_put(signal_cache, signal);

  if (terminate)
    process_destroy(terminate);
}

struct SignalContext {
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
};

void
process_signal_handle(struct Process *current, struct sigaction *handler, struct Signal *signal)
{
  int r;
  struct SignalContext *ctx;

  ctx = ((struct SignalContext *) current->thread->tf->sp) - 1;
  if ((r = vm_space_check_buf(current->vm, ctx, sizeof(*ctx), VM_WRITE)) < 0) {
    spin_unlock(&process_lock);
    object_pool_put(signal_cache, signal);
    process_destroy(SIGSEGV);
  }

  ctx->r0     = current->thread->tf->r0;
  ctx->sp     = current->thread->tf->sp;
  ctx->lr     = current->thread->tf->lr;
  ctx->pc     = current->thread->tf->pc;
  ctx->psr    = current->thread->tf->psr;
  ctx->trapno = current->thread->tf->trapno;
  
  ctx->signo = signal->info.si_signo;
  ctx->handler = (uint32_t) handler->sa_handler;

  current->thread->tf->r0 = (uint32_t) &ctx->signo;
  current->thread->tf->sp = (uintptr_t) ctx;
  current->thread->tf->pc = (uintptr_t) current->signal_stub;

  spin_unlock(&process_lock);
  object_pool_put(signal_cache, signal);
}

void
process_signal_check(void)
{
  struct Process *current = process_current();
  struct ListLink *link;

  spin_lock(&process_lock);

  link = current->pending_signals.next;
  while (link != &current->pending_signals) {
    struct Signal *signal;
    struct sigaction *handler;

    signal = LIST_CONTAINER(current->pending_signals.next, struct Signal, link);
    handler = &current->signal_handlers[signal->info.si_signo];

    // TODO: if blocked, continue

    list_remove(&signal->link);

    if (handler->sa_handler == SIG_DFL)
      return process_signal_default(current, signal);

    return process_signal_handle(current, handler, signal);
  }

  spin_unlock(&process_lock);
}

int
process_signal_return(void)
{
  int r;
  struct Process *current = process_current();
  struct SignalContext *ctx;

  spin_lock(&process_lock);

  ctx = (struct SignalContext *) current->thread->tf->sp;
  if ((r = vm_space_check_buf(current->vm, ctx, sizeof(*ctx), VM_WRITE)) < 0) {
    spin_unlock(&process_lock);
    process_destroy(SIGSEGV);
  }

  current->thread->tf->r0 = ctx->r0;
  current->thread->tf->sp = ctx->sp;
  current->thread->tf->lr = ctx->lr;
  current->thread->tf->pc = ctx->pc;
  current->thread->tf->psr = ctx->psr;
  current->thread->tf->trapno = ctx->trapno;
  
  spin_unlock(&process_lock);
  
  return current->thread->tf->r0;
}

int
process_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
  unsigned long rq_timeout, rm_timeout, start;
  struct KSemaphore sem;
  int r;

  start = ktime_get();

  rq_timeout = rqtp->tv_sec * TICKS_PER_S + rqtp->tv_nsec / NS_PER_TICK;

  ksem_create(&sem, 0);
  r = ksem_get(&sem, rq_timeout, 1);

  rm_timeout = MIN(ktime_get() - start, rq_timeout);

  if (rmtp != NULL) {
    rmtp->tv_sec  = rm_timeout / TICKS_PER_S;
    rmtp->tv_nsec = (rm_timeout % TICKS_PER_S) * NS_PER_TICK;
  }

  ksem_destroy(&sem);

  return r;
}

pid_t
process_get_gid(pid_t pid)
{
  struct Process *process, *current = process_current();
  int r;

  if (pid == 0)
    return current->pgid;

  if (pid < 0)
    return -EINVAL;

  spin_lock(&process_lock);

  if ((process = pid_lookup(pid)) == NULL) {
    r = -ESRCH;
  } else {
    // TODO: restrict access?
    r = process->pgid;
  }

  spin_unlock(&process_lock);

  return r;
}

int
process_set_gid(pid_t pid, pid_t pgid)
{
  struct Process *process, *current = process_current();
  int r;

  if (pid == 0)
    pid = current->pid;
  if (pgid == 0)
    pgid = current->pgid;

  if (pgid < 0)
    return -EINVAL;

  spin_lock(&process_lock);

  if ((process = pid_lookup(pid)) == NULL) {
    r = -ESRCH;
  } else {
    // TODO: restrict access?
    process->pgid = pgid;
    r = 0;
  }

  spin_unlock(&process_lock);

  return r;
}
