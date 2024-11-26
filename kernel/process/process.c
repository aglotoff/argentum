#include <kernel/assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <kernel/armv7/regs.h>
#include <kernel/core/cpu.h>
#include <kernel/console.h>
#include <kernel/elf.h>
#include <kernel/fd.h>
#include <kernel/fs/fs.h>
#include <kernel/hash.h>
#include <kernel/object_pool.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/monitor.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>
#include <kernel/vmspace.h>
#include <kernel/trap.h>
#include <kernel/tick.h>
#include <kernel/semaphore.h>
#include <kernel/core/irq.h>

struct KObjectPool *process_cache;
struct KObjectPool *thread_cache;

// Size of PID hash table
#define NBUCKET   256

// Process ID hash table
static struct {
  struct KListLink table[NBUCKET];
  struct KSpinLock lock;
} pid_hash;

// Lock to protect the parent/child relationships between the processes
struct KListLink __process_list;
struct KSpinLock __process_lock;

static void process_run(void *);
static void arch_trap_frame_pop(struct TrapFrame *);

static struct Process *init_process;

static void
process_ctor(void *buf, size_t size)
{
  struct Process *proc = (struct Process *) buf;

  k_waitqueue_init(&proc->wait_queue);
  k_list_init(&proc->children);
  k_list_init(&proc->signal_queue);

  (void) size;
}

void
process_init(void)
{
  extern uint8_t _binary_obj_user_init_start[];

  process_cache = k_object_pool_create("process_cache", sizeof(struct Process), 0, process_ctor, NULL);
  if (process_cache == NULL)
    panic("cannot allocate process_cache");
  
  HASH_INIT(pid_hash.table);
  k_spinlock_init(&pid_hash.lock, "pid_hash");

  k_list_init(&__process_list);
  k_spinlock_init(&__process_lock, "process_lock");

  // Create the init process
  if (process_create(_binary_obj_user_init_start, &init_process) != 0)
    panic("Cannot create the init process");
}

struct Process *
process_alloc(void)
{
  static pid_t next_pid;
  struct Process *process;

  if ((process = (struct Process *) k_object_pool_get(process_cache)) == NULL)
    return NULL;

  process->thread = k_thread_create(process, process_run, process, NZERO);
  if (process->thread == NULL) {
    k_object_pool_put(process_cache, process);
    return NULL;
  }
  k_list_init(&process->children);
  k_list_null(&process->pid_link);
  k_list_null(&process->link);
  k_list_null(&process->sibling_link);

  process->parent = NULL;
  process->state = PROCESS_STATE_ACTIVE;
  process->flags = 0;

  memset(process->name, 0, 64);

  process->times.tms_utime  = 0;
  process->times.tms_stime  = 0;
  process->times.tms_cutime = 0;
  process->times.tms_cstime = 0;

  k_spinlock_acquire(&pid_hash.lock);

  if ((process->pid = ++next_pid) < 0)
    panic("pid overflow");

  HASH_PUT(pid_hash.table, &process->pid_link, process->pid);

  k_spinlock_release(&pid_hash.lock);

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

int 
arch_trap_frame_init(struct Process *process, uintptr_t entry, uintptr_t arg1,
                     uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  process->thread->tf->r0  = arg1;              // argc
  process->thread->tf->r1  = arg2;              // argv
  process->thread->tf->r2  = arg3;              // environ
  process->thread->tf->sp  = sp;                // stack pointer
  process->thread->tf->psr = PSR_M_USR | PSR_F; // user mode, interrupts enabled
  process->thread->tf->pc  = entry;             // process entry point
  return arg1;
}

static int
process_load_binary(struct Process *proc, const void *binary)
{
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;
  int r;
  uintptr_t addr;

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

    addr = vmspace_map(proc->vm, ph->vaddr, ph->memsz, PROT_READ | PROT_WRITE | PROT_EXEC | VM_USER);
    if (addr != ph->vaddr)
      return (int) addr;

    if ((r = vm_copy_out(proc->vm->pgtab, (uint8_t *) elf + ph->offset,
                         ph->vaddr, ph->filesz)) < 0)
      return r;

    // proc->vm->heap = ROUND_UP(ph->vaddr + ph->memsz, PAGE_SIZE);
  }

  addr = vmspace_map(proc->vm, (VIRT_USTACK_TOP - USTACK_SIZE), USTACK_SIZE,
                        PROT_READ | PROT_WRITE | VM_USER);
  if (addr != (VIRT_USTACK_TOP - USTACK_SIZE))
    return (int) addr;

  return arch_trap_frame_init(proc, elf->entry, 0, 0, 0, VIRT_USTACK_TOP);
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

  process_lock();
  k_list_add_back(&__process_list, &proc->link);
  process_unlock();

  k_thread_resume(proc->thread);

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
  process_lock();
  k_list_remove(&process->link);
  process_unlock();

  k_spinlock_acquire(&pid_hash.lock);
  k_list_remove(&process->pid_link);
  k_spinlock_release(&pid_hash.lock);

  // Return the process descriptor to the cache
  k_object_pool_put(process_cache, process);
}

struct Process *
pid_lookup(pid_t pid)
{
  struct KListLink *l;
  struct Process *proc;

  k_spinlock_acquire(&pid_hash.lock);

  HASH_FOREACH_ENTRY(pid_hash.table, l, pid) {
    proc = KLIST_CONTAINER(l, struct Process, pid_link);
    if (proc->pid == pid) {
      k_spinlock_release(&pid_hash.lock);
      return proc;
    }
  }

  k_spinlock_release(&pid_hash.lock);
  return NULL;
}

void
process_destroy(int status)
{
  struct KListLink *l;
  struct Process *child, *current = process_current();
  int has_zombies;

  if(status)
    cprintf("[k] process #%d destroyed with code 0x%x\n", current->pid, status);

  // Remove the pid hash link
  // TODO: place this code somewhere else?
  k_spinlock_acquire(&pid_hash.lock);
  HASH_REMOVE(&current->pid_link);
  k_spinlock_release(&pid_hash.lock);

  vm_space_destroy(current->vm);

  fd_close_all(current);
  fs_path_put(current->cwd);

  assert(init_process != NULL);

  process_lock();

  // Move children to the init process
  has_zombies = 0;
  while (!k_list_is_empty(&current->children)) {
    l = current->children.next;
    k_list_remove(l);

    child = KLIST_CONTAINER(l, struct Process, sibling_link);
    child->parent = init_process;
    k_list_add_back(&init_process->children, l);

    // Check whether there is a child available to be cleaned up
    if (child->state == PROCESS_STATE_ZOMBIE)
      has_zombies = 1;
  }

  // Wake up the init process to cleanup zombie children
  if (has_zombies)
    k_waitqueue_wakeup_all(&init_process->wait_queue);

  current->state = PROCESS_STATE_ZOMBIE;
  current->flags |= PROCESS_STATUS_AVAILABLE;
  current->status = status;

  // Wakeup the parent process
  if (current->parent)
    k_waitqueue_wakeup_all(&current->parent->wait_queue);

  process_unlock();

  k_thread_exit();
}

pid_t
process_copy(int share_vm)
{
  struct Process *child, *current = process_current();

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->vm = vm_space_clone(current->vm, share_vm)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  process_lock();

  child->parent         = current;
  *child->thread->tf    = *current->thread->tf;
  child->thread->tf->r0 = 0;

  fd_clone(current, child);
  signal_clone(current, child);

  child->pgid  = current->pgid;
  child->ruid  = current->ruid;
  child->euid  = current->euid;
  child->rgid  = current->rgid;
  child->egid  = current->egid;
  child->cmask = current->cmask;
  child->cwd   = fs_path_duplicate(current->cwd);

  k_list_add_back(&__process_list, &child->link);
  k_list_add_back(&current->children, &child->sibling_link);

  process_unlock();

  // cprintf("[k] process #%x created\n", child->pid);

  k_thread_resume(child->thread);

  return child->pid;
}

/**
 * Check whether the given process ID or group ID matches the given argument.
 * 
 * @param process The process
 * @param pid     The PID argument (see waitpid or kill)
 * 
 * @return 1 if the process matches the given PID, 0 otherwise
 */
int
process_match_pid(struct Process *process, pid_t pid)
{
  if (pid == -1)            // Match all
    return 1;
  if (process->pid == pid)  // Exact match
    return 1;
  if (pid < 0)              // Match exact process group ID
    return process->pgid == -pid;
  if (pid == 0)             // Match current process group ID
    return process->pgid == process_current()->pgid;
  return 0;
}

pid_t
process_wait(pid_t pid, int *stat_loc, int options)
{
  struct Process *current = process_current();
  int r;

  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  process_lock();

  for (;;) {
    struct KListLink *l;
    pid_t match = 0;

    KLIST_FOREACH(&current->children, l) {
      struct Process *process;

      process = KLIST_CONTAINER(l, struct Process, sibling_link);

      if (!process_match_pid(process, pid))
        continue;

      // Rememeber that we have at least one match
      match = process->pid;

      if (process->flags & PROCESS_STATUS_AVAILABLE) {
        if ((process->state == PROCESS_STATE_STOPPED) && !(options & WUNTRACED))
          continue;

        // TODO: WCONTINUED
        if (process->state == PROCESS_STATE_ACTIVE)
          continue;

        process->flags &= ~PROCESS_STATUS_AVAILABLE;
        *stat_loc = process->status;

        if (process->state == PROCESS_STATE_ZOMBIE) {
          k_list_remove(&process->sibling_link);
          k_list_remove(&process->link);

          // Include the times of the terminated child process in the parent's
          // times structure. Thus, only the times of children for which wait
          // successfully returns will be included.
          current->times.tms_cutime += process->times.tms_utime;
          current->times.tms_cstime += process->times.tms_stime;

          process_unlock();

          process_free(process);
        } else {
          process_unlock();
        }

        return match;
      }
    }

    if (!match) { // No children matched
      r = -ECHILD;
      break;
    }

    if (options & WNOHANG) {  // Do not sleep
      r = 0;
      break;
    }

    if ((r = k_waitqueue_sleep(&current->wait_queue, &__process_lock)) != 0)
      break;
  }

  process_unlock();

  return r;
}

void 
process_get_times(struct Process *process, struct tms *times)
{
  process_lock();
  memmove(times, &process->times, sizeof process->times);
  process_unlock();
}

static void
process_run(void *arg)
{
  static int first;

  struct Process *process = (struct Process *) arg;

  if (!first) {
    first = 1;

    fs_init();

    if ((process->cwd == NULL) && (fs_lookup("/", 0, &process->cwd) < 0))
      panic("root not found");
  }

  k_irq_disable();

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
  panic("deprecated %d\n", increment);
  return NULL;
}

int
process_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
  unsigned long rq_timeout, rm_timeout, start;
  struct KSemaphore sem;
  int r;

  start = k_tick_get();

  rq_timeout = rqtp->tv_sec * TICKS_PER_SECOND + rqtp->tv_nsec / NS_PER_TICK;

  k_semaphore_init(&sem, 0);
  r = k_semaphore_timed_get(&sem, rq_timeout);

  rm_timeout = MIN(k_tick_get() - start, rq_timeout);

  if (rmtp != NULL) {
    rmtp->tv_sec  = rm_timeout / TICKS_PER_SECOND;
    rmtp->tv_nsec = (rm_timeout % TICKS_PER_SECOND) * NS_PER_TICK;
  }

  k_semaphore_fini(&sem);

  return r == -ETIMEDOUT ? 0 : r;
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

  process_lock();

  if ((process = pid_lookup(pid)) == NULL) {
    r = -ESRCH;
  } else {
    // TODO: restrict access?
    r = process->pgid;
  }

  process_unlock();

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

  process_lock();

  if ((process = pid_lookup(pid)) == NULL) {
    r = -ESRCH;
  } else {
    // TODO: restrict access?
    process->pgid = pgid;
    r = 0;
  }

  process_unlock();

  return r;
}

void
process_update_times(struct Process *process, clock_t user, clock_t system)
{
  process_lock();
  process->times.tms_utime += user;
  process->times.tms_stime += system;
  process_unlock();
}
