#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <kernel/armv7.h>
#include <kernel/cpu.h>
#include <kernel/drivers/console.h>
#include <kernel/drivers/eth.h>
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

static struct KObjectPool *process_pool;

#define NBUCKET   256

HASH_DECLARE(pid_hash, NBUCKET);

static struct {
  int             nprocesses;
  struct ListLink runqueue;
  struct SpinLock lock;
} ptable;

static void process_run(void);
static void process_pop_tf(struct Trapframe *);
static void process_suspend(void);
static void process_resume(struct Process *);
static void scheduler_yield(void);

struct Process *
my_process(void)
{
  struct Process *process;

  irq_save();
  process = my_cpu()->process;
  irq_restore();

  return process;
}

void
process_init(void)
{
  extern uint8_t _binary_obj_user_init_start[];

  process_pool = kobject_pool_create("process_pool", sizeof(struct Process), 0);
  if (process_pool == NULL)
    panic("cannot allocate process_pool");

  HASH_INIT(pid_hash);

  list_init(&ptable.runqueue);
  spin_init(&ptable.lock, "ptable");

  // Create the 'init' process.
  process_create(_binary_obj_user_init_start);
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
  if ((page = page_alloc(0)) == NULL) {
    kobject_free(process_pool, process);
    return NULL;
  }

  process->kstack = (uint8_t *) page2kva(page);
  page->ref_count++;

  sp = process->kstack + PAGE_SIZE;

  // Leave room for Trapframe.
  sp -= sizeof *process->tf;
  process->tf = (struct Trapframe *) sp;

  // Setup new context to start executing at process_run.
  sp -= sizeof *process->context;
  process->context = (struct Context *) sp;
  memset(process->context, 0, sizeof *process->context);
  process->context->lr = (uint32_t) process_run;

  process->state = PROCESS_EMBRIO;
  process->parent = NULL;
  list_init(&process->children);
  process->sibling.next = NULL;
  process->sibling.prev = NULL;

  spin_lock(&ptable.lock);

  if ((process->pid = ++next_pid) < 0)
    panic("pid overflow");

  HASH_PUT(pid_hash, &process->pid_link, process->pid);

  ptable.nprocesses++;

  spin_unlock(&ptable.lock);

  for (i = 0; i < OPEN_MAX; i++)
    process->files[i] = NULL;

  return process;
}

int
process_setup_vm(struct Process *p)
{
  struct PageInfo *trtab_page;

  if ((trtab_page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL)
    return -ENOMEM;

  p->trtab = (tte_t *) page2kva(trtab_page);
  trtab_page->ref_count++;

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

  proc->heap   = 0;
  proc->ustack = USTACK_TOP - USTACK_SIZE;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;

    if (ph->filesz > ph->memsz)
      return -EINVAL;
    
    if ((r = vm_alloc_region(proc->trtab, (void *) ph->vaddr, ph->memsz) < 0))
      return r;

    if ((r = vm_copy_out(proc->trtab, (void *) ph->vaddr, binary + ph->offset,
                         ph->filesz)) < 0)
      return r;

    proc->heap = MAX(proc->heap, ph->vaddr + ph->memsz);
  }

  // Allocate user stack.
  if ((r = vm_alloc_region(proc->trtab, (void *) (proc->ustack),
                           USTACK_SIZE)) < 0)
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
process_create(const void *binary)
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

  spin_lock(&ptable.lock);

  proc->state = PROCESS_RUNNABLE;
  list_add_back(&ptable.runqueue, &proc->link);

  spin_unlock(&ptable.lock);

  return 0;

fail3:
  vm_free(proc->trtab);
fail2:
  process_free(proc);
fail1:
  return r;
}

void
process_free(struct Process *proc)
{
  struct PageInfo *kstack_page;

  kstack_page = kva2page(proc->kstack);
  kstack_page->ref_count--;
  page_free(kstack_page);

  spin_lock(&ptable.lock);
  HASH_REMOVE(&proc->pid_link);
  spin_unlock(&ptable.lock);

  kobject_free(process_pool, proc);
}

struct Process *
pid_lookup(pid_t pid)
{
  struct ListLink *l;
  struct Process *proc;

  HASH_FOREACH_ENTRY(pid_hash, l, pid) {
    proc = LIST_CONTAINER(l, struct Process, pid_link);
    if (proc->pid == pid)
      return proc;
  }

  return NULL;
}

void
process_destroy(int status)
{
  struct Process *current = my_process();
  int fd;
  
  // if (status == 0)
  //   cprintf("[%08x] exit with code %d\n", current->pid, status);
  // else
  //   cprintf("[%08x] exit with code %d\n", current->pid, status);

  vm_free(current->trtab);

  for (fd = 0; fd < OPEN_MAX; fd++)
    if (current->files[fd])
      file_close(current->files[fd]);

  fs_inode_put(current->cwd);

  current->exit_code = status;

  spin_lock(&ptable.lock);

  current->state = PROCESS_ZOMBIE;
  ptable.nprocesses--;

  if (current->parent && (current->parent->state == PROCESS_SLEEPING))
    process_resume(current->parent);

  scheduler_yield();
}

pid_t
process_copy(void)
{
  struct Process *child, *parent = my_process();
  int fd;

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->trtab = vm_copy(parent->trtab)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->heap   = parent->heap;
  child->ustack = parent->ustack;
  child->parent = parent;
  *child->tf    = *parent->tf;
  child->tf->r0 = 0;

  for (fd = 0; fd < OPEN_MAX; fd++) {
    child->files[fd] = parent->files[fd] ? file_dup(parent->files[fd]) : NULL;
  }

  child->cwd = fs_inode_dup(parent->cwd);

  spin_lock(&ptable.lock);

  list_add_back(&parent->children, &child->sibling);

  child->state = PROCESS_RUNNABLE;
  list_add_back(&ptable.runqueue, &child->link);

  spin_unlock(&ptable.lock);

  return child->pid;
}

static void
scheduler_yield(void)
{
  int irq_flags;

  irq_flags = my_cpu()->irq_flags;
  context_switch(&my_process()->context, my_cpu()->scheduler);
  my_cpu()->irq_flags = irq_flags;
}

void
scheduler(void)
{
  struct ListLink *link;
  struct Process *next;

  for (;;) {
    irq_enable();

    spin_lock(&ptable.lock);

    while (!list_empty(&ptable.runqueue)) {
      link = ptable.runqueue.next;
      list_remove(link);

      next = LIST_CONTAINER(link, struct Process, link);
      assert(next->state == PROCESS_RUNNABLE);

      next->state = PROCESS_RUNNING;
      my_cpu()->process = next;

      vm_switch_user(next->trtab);
      context_switch(&my_cpu()->scheduler, next->context);
    }

    // If there are no more proceses to run, drop into the kernel monitor.
    if ((ptable.nprocesses == 0) && (cpu_id() == 0)) {
      spin_unlock(&ptable.lock);
      
      cprintf("No runnable processes in the system!\n");
      for (;;)
        monitor(NULL);
    }

    // Mark that no process is running on this CPU.
    my_cpu()->process = NULL;
    vm_switch_kernel();

    spin_unlock(&ptable.lock);

    asm volatile("wfi");
  }
}

void
process_yield(void)
{
  struct Process *current;
  
  spin_lock(&ptable.lock);

  current = my_cpu()->process;

  current->state = PROCESS_RUNNABLE;
  list_add_back(&ptable.runqueue, &current->link);

  scheduler_yield();

  spin_unlock(&ptable.lock);
}

static void
process_suspend(void)
{
  assert(spin_holding(&ptable.lock));

  my_process()->state = PROCESS_SLEEPING;
  scheduler_yield();
}

static void
process_resume(struct Process *proc)
{
  assert(spin_holding(&ptable.lock));

  proc->state = PROCESS_RUNNABLE;

  list_remove(&proc->link);
  list_add_back(&ptable.runqueue, &proc->link);
}

pid_t
process_wait(pid_t pid, int *stat_loc, int options)
{
  struct Process *p, *current = my_process();
  struct ListLink *l;
  int r, found;

  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  spin_lock(&ptable.lock);

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

      if (p->state == PROCESS_ZOMBIE) {
        list_remove(&p->sibling);

        spin_unlock(&ptable.lock);

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

    process_suspend();
  }

  spin_unlock(&ptable.lock);

  return r;
}

// A process' very first scheduling by scheduler() will switch here.
static void
process_run(void)
{
  static int first;

  struct Process *proc = my_process();

  // Still holding the process table lock.
  spin_unlock(&ptable.lock);

  if (!first) {
    first = 1;

    fs_init();

    if ((proc->cwd == NULL) && (fs_name_lookup("/", &proc->cwd) < 0))
      panic("root not found");

    eth_write("This is my test packet!", 24);
  }

  // "Return" to the user space.
  process_pop_tf(proc->tf);
}

// Restores the register values in the Trapframe.
// This function doesn't return.
static void
process_pop_tf(struct Trapframe *tf)
{
  asm volatile(
    "mov     sp, %0\n"
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

void
process_sleep(struct ListLink *q, struct SpinLock *lock)
{
  struct Process *current = my_process();

  if (lock != &ptable.lock) {
    spin_lock(&ptable.lock);
    spin_unlock(lock);
  }

  list_add_back(q, &current->link);
  process_suspend();

  if (lock != &ptable.lock) {
    spin_unlock(&ptable.lock);
    spin_lock(lock);
  }
}

void
process_wakeup(struct ListLink *q)
{
  struct ListLink *l;
  struct Process *p;

  spin_lock(&ptable.lock);
  while (!list_empty(q)) {
    l = q->next;
    list_remove(l);

    p = LIST_CONTAINER(l, struct Process, link);
    process_resume(p);
  }
  spin_unlock(&ptable.lock);
}

static int
copy_args(tte_t *trtab, char *const args[], uintptr_t limit, char **sp)
{
  char *oargs[32];
  char *p;
  size_t n;
  int i, r;
  
  for (p = *sp, i = 0; args[i] != NULL; i++) {
    if (i >= 32)
      return -E2BIG;

    n = strlen(args[i]);
    p -= ROUND_UP(n + 1, sizeof(uint32_t));

    if (p < (char *) limit)
      return -E2BIG;

    if ((r = vm_copy_out(trtab, p, args[i], n)) < 0)
      return r;

    oargs[i] = p;
  }
  oargs[i] = NULL;

  n = (i + 1) * sizeof(char *);
  p -= ROUND_UP(n, sizeof(uint32_t));
  
  if (p < (char *) (USTACK_TOP - USTACK_SIZE))
    return -E2BIG;

  if ((r = vm_copy_out(trtab, p, oargs, n)) < 0)
    return r;

  *sp = p;

  return i;
}

int
process_exec(const char *path, char *const argv[], char *const envp[])
{
  struct PageInfo *trtab_page;
  struct Process *proc;
  struct Inode *ip;
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  off_t off;
  tte_t *trtab;
  uintptr_t heap, ustack;
  char *usp, *uargv, *uenvp;
  int r, argc;

  if ((r = fs_name_lookup(path, &ip)) < 0)
    return r;

  fs_inode_lock(ip);

  if (!S_ISREG(ip->mode)) {
    r = -ENOENT;
    goto out1;
  }

  if ((trtab_page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL) {
    r = -ENOMEM;
    goto out1;
  }

  trtab = (tte_t *) page2kva(trtab_page);
  trtab_page->ref_count++;

  if ((r = fs_inode_read(ip, &elf, sizeof(elf), 0)) != sizeof(elf))
    goto out2;
  
  if (memcmp(elf.ident, "\x7f""ELF", 4) != 0) {
    r = -EINVAL;
    goto out2;
  }

  heap   = 0;
  ustack = USTACK_TOP - USTACK_SIZE;

  for (off = elf.phoff;
       (size_t) off < elf.phoff + elf.phnum * sizeof(ph);
       off += sizeof(ph)) {
    if ((r = fs_inode_read(ip, &ph, sizeof(ph), off)) != sizeof(ph))
      goto out2;

    if (ph.type != PT_LOAD)
      continue;

    if (ph.filesz > ph.memsz) {
      r = -EINVAL;
      goto out2;
    }

    if ((ph.vaddr >= KERNEL_BASE) || (ph.vaddr + ph.memsz > KERNEL_BASE)) {
      r = -EINVAL;
      goto out2;
    }

    if ((r = vm_alloc_region(trtab, (void *) ph.vaddr, ph.memsz) < 0))
      goto out2;

    if ((r = vm_load(trtab, (void *) ph.vaddr, ip, ph.filesz, ph.offset)) < 0)
      goto out2;

    heap = MAX(heap, ph.vaddr + ph.memsz);
  }

  // Allocate user stack.
  if ((r = vm_alloc_region(trtab, (void *) ustack, USTACK_SIZE)) < 0)
    return r;

  // Copy args and environment.
  usp = (char *) USTACK_TOP;
  if ((r = copy_args(trtab, argv, ustack, &usp)) < 0)
    goto out2;

  argc = r;
  uargv = usp;

  if ((r = copy_args(trtab, envp, ustack, &usp)) < 0)
    goto out2;

  uenvp = usp;

  fs_inode_unlock(ip);
  fs_inode_put(ip);

  proc = my_process();

  vm_switch_user(trtab);
  vm_free(proc->trtab);

  proc->trtab  = trtab;
  proc->heap   = heap;
  proc->ustack = ustack;

  proc->tf->r0     = argc;                // arg #0: argc
  proc->tf->r1     = (uint32_t) uargv;    // arg #1: argv
  proc->tf->r2     = (uint32_t) uenvp;    // arg #2: environ
  proc->tf->sp_usr = (uint32_t) usp;      // stack pointer
  proc->tf->pc     = elf.entry;           // process entry point

  return argc;

out2:
  vm_free(trtab);

out1:
  fs_inode_unlock(ip);
  fs_inode_put(ip);

  return r;
}

void *
process_grow(ptrdiff_t increment)
{
  struct Process *current = my_process();
  uintptr_t o, n;

  o = ROUND_UP(current->heap, sizeof(uintptr_t));
  n = ROUND_UP(o + increment, sizeof(uintptr_t));

  if (increment > 0) {
    if ((n < o) || (n > (current->ustack + PAGE_SIZE)))
      // Overflow
      return (void *) -1;
    if (vm_alloc_region(current->trtab, (void *) ROUND_UP(o, PAGE_SIZE),
                        ROUND_UP(n, PAGE_SIZE) - ROUND_UP(o, PAGE_SIZE)) != 0)
      return (void *) -1;
  } else if (increment < 0) {
    if (n > o)
      // Overflow
      return (void *) -1;
    if (vm_dealloc_region(current->trtab, (void *) ROUND_UP(n, PAGE_SIZE),
                          ROUND_UP(o, PAGE_SIZE) - ROUND_UP(n, PAGE_SIZE)) != 0)
      return (void *) -1;
  }

  current->heap = n;

  vm_switch_user(current->trtab);

  return (void *) o;
}
