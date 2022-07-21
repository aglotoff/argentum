#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <armv7.h>
#include <cpu.h>
#include <cprintf.h>
#include <elf.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <hash.h>
#include <mm/kobject.h>
#include <mm/page.h>
#include <mm/vm.h>
#include <monitor.h>
#include <sync.h>
#include <trap.h>

#include <process.h>

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
static void process_pop_tf(struct TrapFrame *);

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

  struct Page *page;
  struct Process *process;
  uint8_t *sp;

  process = (struct Process *) kobject_alloc(process_pool);

  // Allocate per-process kernel stack
  if ((page = page_alloc_one(0)) == NULL)
    goto fail1;

  process->kstack = (uint8_t *) page2kva(page);
  page->ref_count++;

  sp = process->kstack + PAGE_SIZE;

  // Leave room for Trapframe.
  sp -= sizeof *process->tf;
  process->tf = (struct TrapFrame *) sp;

  // Setup new context to start executing at task_run.
  if ((process->task = task_create(process, process_run, sp)) == NULL)
    goto fail2;

  process->parent = NULL;
  process->zombie = 0;
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
  page_free_one(page);
fail1:
  kobject_free(process_pool, process);
  return NULL;
}

int
process_setup_vm(struct Process *proc)
{
  if ((proc->vm = vm_create()) == NULL)
    return -ENOMEM;

  return 0;
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

  proc->heap  = 0;
  proc->stack = USTACK_TOP - USTACK_SIZE;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;

    if (ph->filesz > ph->memsz)
      return -EINVAL;
    
    if ((r = vm_user_alloc(proc->vm, (void *) ph->vaddr, ph->memsz,
                             VM_READ | VM_WRITE | VM_EXEC | VM_USER) < 0))
      return r;

    if ((r = vm_user_copy_out(proc->vm, (void *) ph->vaddr,
                         (uint8_t *) elf + ph->offset, ph->filesz)) < 0)
      return r;

    proc->heap = MAX(proc->heap, ph->vaddr + ph->memsz);
  }

  if ((r = vm_user_alloc(proc->vm, (void *) (proc->stack), USTACK_SIZE,
                           VM_READ | VM_WRITE | VM_USER) < 0))
    return r;

  proc->tf->r0  = 0;                   // argc
  proc->tf->r1  = 0;                   // argv
  proc->tf->r2  = 0;                   // environ
  proc->tf->sp  = USTACK_TOP;          // stack pointer
  proc->tf->psr = PSR_M_USR | PSR_F;   // user mode, interrupts enabled
  proc->tf->pc  = elf->entry;          // process entry point

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

  proc->uid   = 0;
  proc->gid   = 0;
  proc->cmask = 0;

  task_enqueue(proc->task);

  if (pstore != NULL)
    *pstore = proc;

  return 0;

fail3:
  vm_destroy(proc->vm);
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
  struct Page *kstack_page;

  // Destroy the task descriptor
  task_destroy(process->task);

  // Free the kernel stack
  kstack_page = kva2page(process->kstack);
  kstack_page->ref_count--;
  page_free_one(kstack_page);

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

  vm_destroy(current->vm);

  for (fd = 0; fd < OPEN_MAX; fd++)
    if (current->files[fd])
      file_close(current->files[fd]);

  fs_inode_put(current->cwd);

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
    if (child->zombie)
      has_zombies = 1;
  }

  // Wake up the init process to cleanup zombie children
  if (has_zombies)
    task_wakeup(&init_process->wait_queue);

  current->zombie = 1;
  current->exit_code = status;

  // Wakeup the parent process
  if (current->parent)
    task_wakeup(&current->parent->wait_queue);

  task_sleep(&current->wait_queue, &process_lock);
}

pid_t
process_copy(void)
{
  struct Process *child, *current = my_process();
  int fd;

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->vm = vm_clone(current->vm)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->heap  = current->heap;
  child->stack = current->stack;
  child->parent = current;
  *child->tf    = *current->tf;
  child->tf->r0 = 0;

  for (fd = 0; fd < OPEN_MAX; fd++) {
    child->files[fd] = current->files[fd] ? file_dup(current->files[fd]) : NULL;
  }

  child->uid   = current->uid;
  child->gid   = current->gid;
  child->cmask = current->cmask;
  child->cwd   = fs_inode_dup(current->cwd);

  spin_lock(&process_lock);
  list_add_back(&current->children, &child->sibling);
  spin_unlock(&process_lock);

  task_enqueue(child->task);

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

      if (p->zombie) {
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

    task_sleep(&current->wait_queue, &process_lock);
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

// Load the user-mode registers from the trap frame.
// Doesn't return.
static void
process_pop_tf(struct TrapFrame *tf)
{
  asm volatile(
    // Setup the stack pointer
    "mov     sp, %0\n"

    // This is the same code as at the end of trap_user:
    "ldr     lr, [sp], #16\n"
    "msr     spsr, lr\n"
    "ldmdb   sp, {sp,lr}^\n"
    "ldmia   sp!, {r0-r12,pc}^\n"
    :
    : "r" (tf)
    : "memory"
  );
}

void *
process_grow(ptrdiff_t increment)
{
  struct Process *current = my_process();
  uintptr_t o, n;

  o = ROUND_UP(current->heap, sizeof(uintptr_t));
  n = ROUND_UP(o + increment, sizeof(uintptr_t));

  if (increment > 0) {
    if ((n < o) || (n > (current->stack + PAGE_SIZE)))
      // Overflow
      return (void *) -1;
    if (vm_user_alloc(current->vm, (void *) ROUND_UP(o, PAGE_SIZE),
                        ROUND_UP(n, PAGE_SIZE) - ROUND_UP(o, PAGE_SIZE),
                        VM_READ | VM_WRITE | VM_USER) != 0)
      return (void *) -1;
  } else if (increment < 0) {
    if (n > o)
      // Overflow
      return (void *) -1;
    vm_user_dealloc(current->vm, (void *) ROUND_UP(n, PAGE_SIZE),
                          ROUND_UP(o, PAGE_SIZE) - ROUND_UP(n, PAGE_SIZE));
  }

  current->heap = n;

  return (void *) o;
}
