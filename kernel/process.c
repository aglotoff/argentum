#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <argentum/armv7/regs.h>
#include <argentum/cpu.h>
#include <argentum/cprintf.h>
#include <argentum/elf.h>
#include <argentum/fs/file.h>
#include <argentum/fs/fs.h>
#include <argentum/hash.h>
#include <argentum/mm/kmem.h>
#include <argentum/mm/page.h>
#include <argentum/mm/vm.h>
#include <argentum/monitor.h>
#include <argentum/process.h>
#include <argentum/spinlock.h>
#include <argentum/trap.h>

struct KMemCache *process_cache;
struct KMemCache *thread_cache;

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

static void
process_ctor(void *buf, size_t size)
{
  struct Process *proc = (struct Process *) buf;

  wchan_init(&proc->wait_queue);
  list_init(&proc->children);

  (void) size;
}

void
process_init(void)
{
  extern uint8_t _binary_obj_user_init_start[];

  process_cache = kmem_cache_create("process_cache", sizeof(struct Process), 0, process_ctor, NULL);
  if (process_cache == NULL)
    panic("cannot allocate process_cache");
  thread_cache = kmem_cache_create("thread_cache", sizeof(struct ProcessThread), 0, NULL, NULL);
  if (thread_cache == NULL)
    panic("cannot allocate thread cache");

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
  struct ProcessThread *thread;
  uint8_t *sp;

  if ((process = (struct Process *) kmem_alloc(process_cache)) == NULL)
    return NULL;
  
  if ((thread = (struct ProcessThread *) kmem_alloc(thread_cache)) == NULL)
    goto fail1;

  process->thread = thread;

  // Allocate per-process kernel stack
  if ((page = page_alloc_one(0)) == NULL)
    goto fail2;

  process->thread->kstack = (uint8_t *) page2kva(page);
  page->ref_count++;

  sp = process->thread->kstack + PAGE_SIZE;

  // Leave room for the trapframe
  sp -= sizeof *process->thread->tf;
  // The user-mode trapframe will always be on the same stack address
  process->thread->tf = (struct TrapFrame *) sp;

  // Setup new context to start executing at thread_run.
  if (kthread_init(process, &thread->kernel_thread, process_run, NZERO, sp) != 0)
    goto fail3;

  process->parent = NULL;
  process->zombie = 0;
  process->sibling_link.next = NULL;
  process->sibling_link.prev = NULL;

  spin_lock(&pid_hash.lock);

  if ((process->pid = ++next_pid) < 0)
    panic("pid overflow");

  HASH_PUT(pid_hash.table, &process->pid_link, process->pid);

  spin_unlock(&pid_hash.lock);

  for (i = 0; i < OPEN_MAX; i++)
    process->files[i] = NULL;

  return process;

fail3:
  page->ref_count--;
  page_free_one(page);
fail2:
  kmem_free(thread_cache, thread);
fail1:
  kmem_free(process_cache, process);
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
  void *a;

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
    
    a = vm_mmap(proc->vm, (void *) ph->vaddr, ph->memsz,
                VM_READ | VM_WRITE | VM_EXEC | VM_USER);
    if ((int) a < 0)
      return (int) a;

    if ((void *) ph->vaddr != a)
      return -EINVAL;

    if ((r = vm_user_copy_out(proc->vm, (void *) ph->vaddr,
                         (uint8_t *) elf + ph->offset, ph->filesz)) < 0)
      return r;
  }

  if ((r = (int) vm_mmap(proc->vm, (void *) (VIRT_USTACK_TOP - USTACK_SIZE), USTACK_SIZE,
                           VM_READ | VM_WRITE | VM_USER) < 0))
    return r;

  proc->thread->tf->r0  = 0;                   // argc
  proc->thread->tf->r1  = 0;                   // argv
  proc->thread->tf->r2  = 0;                   // environ
  proc->thread->tf->sp  = VIRT_USTACK_TOP;          // stack pointer
  proc->thread->tf->psr = PSR_M_USR | PSR_F;   // user mode, interrupts enabled
  proc->thread->tf->pc  = elf->entry;          // process entry point

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

  kthread_resume(&proc->thread->kernel_thread);

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

void
process_thread_free(struct KThread *kernel_thread)
{
  struct ProcessThread *thread = (struct ProcessThread *) kernel_thread;
  struct Page *kstack_page;

  // Free the kernel stack
  kstack_page = kva2page(thread->kstack);
  kstack_page->ref_count--;
  page_free_one(kstack_page);

  kmem_free(thread_cache, thread);
}

/**
 * Free all resources associated with a process.
 * 
 * @param proc The process to be freed.
 */
void
process_free(struct Process *process)
{
  // Remove the pid hash link
  spin_lock(&pid_hash.lock);
  HASH_REMOVE(&process->pid_link);
  spin_unlock(&pid_hash.lock);

  // Return the process descriptor to the cache
  kmem_free(process_cache, process);
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

  kthread_destroy(&current->thread->kernel_thread);
}

pid_t
process_copy(void)
{
  struct Process *child, *current = process_current();
  int fd;

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->vm = vm_clone(current->vm)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->parent    = current;
  *child->thread->tf       = *current->thread->tf;
  child->thread->tf->r0    = 0;

  for (fd = 0; fd < OPEN_MAX; fd++) {
    child->files[fd] = current->files[fd] ? file_dup(current->files[fd]) : NULL;
  }

  child->uid   = current->uid;
  child->gid   = current->gid;
  child->cmask = current->cmask;
  child->cwd   = fs_inode_dup(current->cwd);

  spin_lock(&process_lock);
  list_add_back(&current->children, &child->sibling_link);
  spin_unlock(&process_lock);

  kthread_resume(&child->thread->kernel_thread);

  return child->pid;
}

pid_t
process_wait(pid_t pid, int *stat_loc, int options)
{
  struct Process *p, *current = process_current();
  struct ListLink *l;
  int r, found;

  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  spin_lock(&process_lock);

  for (;;) {
    found = 0;

    LIST_FOREACH(&current->children, l) {
      p = LIST_CONTAINER(l, struct Process, sibling_link);

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
        list_remove(&p->sibling_link);

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

    wchan_sleep(&current->wait_queue, &process_lock);
  }

  spin_unlock(&process_lock);

  return r;
}

static void
process_run(void)
{
  static int first;

  struct Process *proc = process_current();

  if (!first) {
    first = 1;

    fs_init();

    if ((proc->cwd == NULL) && (fs_name_lookup("/", &proc->cwd) < 0))
      panic("root not found");
  }

  // "Return" to the user space.
  process_pop_tf(proc->thread->tf);
}

// Load the user-mode registers from the trap frame.
// Doesn't return.
static void
process_pop_tf(struct TrapFrame *tf)
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

  return vm_mmap(current->vm, (void *) 0, increment, VM_READ | VM_WRITE | VM_USER);
}
