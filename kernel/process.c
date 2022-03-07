#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <kernel/armv7.h>
#include <kernel/cpu.h>
#include <kernel/cprintf.h>
#include <kernel/elf.h>
#include <kernel/fs/file.h>
#include <kernel/fs/fs.h>
#include <kernel/hash.h>
#include <kernel/mm/kobject.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>
#include <kernel/monitor.h>
#include <kernel/sync.h>
#include <kernel/trap.h>

#include <kernel/process.h>

// Process Object Pool
struct KObjectPool * process_pool;

// Size of PID hash table
#define NBUCKET   256

// Process ID hash table
static struct {
  struct ListLink table[NBUCKET];
  struct SpinLock lock;
} pid_hash;

// Lock to protect the parent/child relationships between the processes
static struct SpinLock process_lock;

static void process_run(void);
static void process_pop_tf(struct UTrapframe *);

static struct Process *init_process;

void
process_init(void)
{
  extern uint8_t _binary_obj_user_init_start[];

  process_pool = kobject_pool_create("process_pool", sizeof(struct Process), 0);
  if (process_pool == NULL)
    panic("cannot allocate process_pool");

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

  struct PageInfo *page;
  struct Process *process;
  uint8_t *sp;

  process = (struct Process *) kobject_alloc(process_pool);

  // Allocate per-process kernel stack
  if ((page = page_alloc(0)) == NULL)
    goto fail1;

  process->kstack = (uint8_t *) page2kva(page);
  page->ref_count++;

  sp = process->kstack + PAGE_SIZE;

  // Leave room for Trapframe.
  sp -= sizeof *process->tf;
  process->tf = (struct UTrapframe *) sp;

  // Setup new context to start executing at thread_run.
  if ((process->thread = thread_create(process, process_run, sp)) == NULL)
    goto fail2;

  process->parent = NULL;
  list_init(&process->wait_queue);
  list_init(&process->children);
  process->sibling.next = NULL;
  process->sibling.prev = NULL;

  spin_lock(&pid_hash.lock);

  if ((process->pid = ++next_pid) < 0)
    panic("pid overflow");

  HASH_PUT(pid_hash.table, &process->pid_link, process->pid);

  spin_unlock(&pid_hash.lock);

  for (i = 0; i < OPEN_MAX; i++)
    process->files[i] = NULL;

  return process;

fail2:
  page->ref_count--;
  page_free(page);
fail1:
  kobject_free(process_pool, process);
  return NULL;
}

int
process_setup_vm(struct Process *p)
{
  struct PageInfo *trtab_page;

  if ((trtab_page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  p->vm.trtab = (tte_t *) page2kva(trtab_page);
  trtab_page->ref_count++;

  return 0;
}

static int
process_load_binary(struct Process *proc, const void *binary)
{
  Elf32_Ehdr *elf;
  int r;

  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0)
    return -EINVAL;

  if ((r = vm_load_binary(&proc->vm, elf)) != 0)
    return r;

  proc->tf->r0     = 0;                   // argc
  proc->tf->r1     = 0;                   // argv
  proc->tf->r2     = 0;                   // environ
  proc->tf->sp_usr = USTACK_TOP;          // stack pointer
  proc->tf->psr    = PSR_M_USR | PSR_F;   // user mode, interrupts enabled
  proc->tf->pc     = elf->entry;          // process entry point

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

  thread_enqueue(proc->thread);

  if (pstore != NULL)
    *pstore = proc;

  return 0;

fail3:
  vm_free(proc->vm.trtab);
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
  struct PageInfo *kstack_page;

  // Destroy the thread descriptor
  thread_destroy(process->thread);

  // Free the kernel stack
  kstack_page = kva2page(process->kstack);
  kstack_page->ref_count--;
  page_free(kstack_page);

  // Remove the pid hash link
  spin_lock(&pid_hash.lock);
  HASH_REMOVE(&process->pid_link);
  spin_unlock(&pid_hash.lock);

  // Return the process descriptor to the pool
  kobject_free(process_pool, process);
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
  struct Process *child, *current = my_process();
  int fd, has_zombies;

  vm_free(current->vm.trtab);

  for (fd = 0; fd < OPEN_MAX; fd++)
    if (current->files[fd])
      file_close(current->files[fd]);

  fs_inode_put(current->cwd);

  current->exit_code = status;

  assert(init_process != NULL);

  spin_lock(&process_lock);

  // Move children to the init process
  has_zombies = 0;
  while (!list_empty(&current->children)) {
    l = current->children.next;
    list_remove(l);

    child = LIST_CONTAINER(l, struct Process, sibling);
    child->parent = init_process;
    list_add_back(&init_process->children, l);

    // Check whether there is a child available to be cleaned up
    if (child->thread->state == THREAD_DESTROYED)
      has_zombies = 1;
  }

  // Wake up the init process to cleanup zombie children
  if (has_zombies)
    thread_wakeup(&init_process->wait_queue);

  // Wakeup the parent process
  if (current->parent)
    thread_wakeup(&current->parent->wait_queue);

  thread_sleep(&current->wait_queue, &process_lock, THREAD_DESTROYED);
}

pid_t
process_copy(void)
{
  struct Process *child, *current = my_process();
  int fd;

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->vm.trtab = vm_copy(current->vm.trtab)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->vm.heap  = current->vm.heap;
  child->vm.stack = current->vm.stack;
  child->parent = current;
  *child->tf    = *current->tf;
  child->tf->r0 = 0;

  for (fd = 0; fd < OPEN_MAX; fd++) {
    child->files[fd] = current->files[fd] ? file_dup(current->files[fd]) : NULL;
  }

  child->cwd = fs_inode_dup(current->cwd);

  spin_lock(&process_lock);
  list_add_back(&current->children, &child->sibling);
  spin_unlock(&process_lock);

  thread_enqueue(child->thread);

  return child->pid;
}

pid_t
process_wait(pid_t pid, int *stat_loc, int options)
{
  struct Process *p, *current = my_process();
  struct ListLink *l;
  int r, found;

  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  spin_lock(&process_lock);

  for (;;) {
    found = 0;

    LIST_FOREACH(&current->children, l) {
      p = LIST_CONTAINER(l, struct Process, sibling);

      // Check whether the current child satisifies the criteria.
      if ((pid > 0) && (p->pid != pid)) {
        continue;
      } else if (pid == 0) {
        // TODO: compare the child group ID to the parent group ID
        break;
      } else if (pid < -1) {
        // TODO: compare the child group ID to (-pid)
        break;
      }

      found = p->pid;

      if (p->thread->state == THREAD_DESTROYED) {
        list_remove(&p->sibling);

        spin_unlock(&process_lock);

        if (stat_loc)
          *stat_loc = p->exit_code;

        process_free(p);

        return found;
      }
    }

    if (!found) {
      r = -ECHILD;
      break;
    } else if (options & WNOHANG) {
      r = 0;
      break;
    }

    thread_sleep(&current->wait_queue, &process_lock, THREAD_SLEEPING);
  }

  spin_unlock(&process_lock);

  return r;
}

static void
process_run(void)
{
  static int first;

  struct Process *proc = my_process();

  if (!first) {
    first = 1;

    fs_init();

    if ((proc->cwd == NULL) && (fs_name_lookup("/", &proc->cwd) < 0))
      panic("root not found");
  }

  // "Return" to the user space.
  process_pop_tf(proc->tf);
}

// Restores the register values in the Trapframe.
// This function doesn't return.
static void
process_pop_tf(struct UTrapframe *tf)
{
  asm volatile(
    "mov     sp, %0\n"
    "vldmia  sp!, {s0-s31}\n"
    "ldmia   sp!, {r1}\n"
    "vmsr    fpscr, r1\n"
    "add     sp, #12\n"                 // skip trapno
    "ldmdb   sp, {sp,lr}^\n"            // restore user mode sp and lr
    "ldmia   sp!, {r0-r12,lr}\n"        // restore context
    "msr     spsr_c, lr\n"              // restore spsr
    "ldmia   sp!, {lr,pc}^\n"           // return
    :
    : "r" (tf)
    : "memory"
  );

  panic("should not return");
}

void *
process_grow(ptrdiff_t increment)
{
  struct Process *current = my_process();
  uintptr_t o, n;

  o = ROUND_UP(current->vm.heap, sizeof(uintptr_t));
  n = ROUND_UP(o + increment, sizeof(uintptr_t));

  if (increment > 0) {
    if ((n < o) || (n > (current->vm.stack + PAGE_SIZE)))
      // Overflow
      return (void *) -1;
    if (vm_alloc_region(current->vm.trtab, (void *) ROUND_UP(o, PAGE_SIZE),
                        ROUND_UP(n, PAGE_SIZE) - ROUND_UP(o, PAGE_SIZE),
                        VM_READ | VM_WRITE | VM_USER) != 0)
      return (void *) -1;
  } else if (increment < 0) {
    if (n > o)
      // Overflow
      return (void *) -1;
    vm_dealloc_region(current->vm.trtab, (void *) ROUND_UP(n, PAGE_SIZE),
                          ROUND_UP(o, PAGE_SIZE) - ROUND_UP(n, PAGE_SIZE));
  }

  current->vm.heap = n;

  return (void *) o;
}
