#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#include "armv7.h"
#include "console.h"
#include "cpu.h"
#include "elf.h"
#include "hash.h"
#include "kobject.h"
#include "mmu.h"
#include "monitor.h"
#include "page.h"
#include "process.h"
#include "spinlock.h"
#include "trap.h"
#include "vm.h"

static struct KObjectPool *process_pool;

#define NBUCKET   256

HASH_DECLARE(pid_hash, NBUCKET);

static struct {
  int             nprocesses;
  struct ListLink runqueue;
  struct Spinlock lock;
} ptable;

static void process_run(void);
static void process_pop_tf(struct Trapframe *);
static void process_sleep(void);
static void process_wakeup(struct Process *);
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
  process_pool = kobject_pool_create("process_pool", sizeof(struct Process), 0);
  if (process_pool == NULL)
    panic("cannot allocate process_pool");

  HASH_INIT(pid_hash);

  list_init(&ptable.runqueue);
  spin_init(&ptable.lock, "ptable");
}

struct Process *
process_alloc(void)
{
  static pid_t next_pid;

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

  cprintf("[%08x] spawn process %08x\n",
          my_process() ? my_process()->pid : 0,
          process->pid);

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

    proc->size = ph->vaddr + ph->memsz;
  }

  // Allocate user stack.
  if ((r = vm_alloc_region(proc->trtab, (void *) (USTACK_TOP - USTACK_SIZE),
                           USTACK_SIZE)) < 0)
    return r;

  proc->tf->r0     = 0;                   // argc
  proc->tf->r1     = 0;                   // argv
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
  
  if (status == 0)
    cprintf("[%08x] exit with code %d\n", current->pid, status);
  else
    cprintf("[%08x] exit with code %d\n", current->pid, status);

  vm_free(current->trtab);

  current->exit_code = status;

  spin_lock(&ptable.lock);

  current->state = PROCESS_ZOMBIE;
  ptable.nprocesses--;

  if (current->parent && (current->parent->state == PROCESS_SLEEPING))
    process_wakeup(current->parent);

  scheduler_yield();
}

pid_t
process_copy(void)
{
  struct Process *child, *parent = my_process();

  if ((child = process_alloc()) == NULL)
    return -ENOMEM;

  if ((child->trtab = vm_copy(parent->trtab)) == NULL) {
    process_free(child);
    return -ENOMEM;
  }

  child->size   = parent->size;
  child->parent = parent;
  *child->tf    = *parent->tf;
  child->tf->r0 = 0;

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
  context_switch(&my_process()->context, my_cpu()->scheduler);
}

void
scheduler(void)
{
  struct ListLink *link;
  struct Process *next;
  
  spin_lock(&ptable.lock);

  for (;;) {
    if (!list_empty(&ptable.runqueue)) {
      link = ptable.runqueue.next;
      list_remove(link);

      next = LIST_CONTAINER(link, struct Process, link);
      assert(next->state == PROCESS_RUNNABLE);

      next->state = PROCESS_RUNNING;
      my_cpu()->process = next;

      vm_switch_user(next->trtab);
      context_switch(&my_cpu()->scheduler, next->context);
    } else {
      // If there are no more proceses to run, drop into the kernel monitor.
      if (ptable.nprocesses == 0) {
        cprintf("No runnable processes in the system!\n");
        for (;;)
          monitor(NULL);
      }

      // Mark that no process is running on this CPU.
      my_cpu()->process = NULL;
      vm_switch_kernel();

      spin_unlock(&ptable.lock);
      spin_lock(&ptable.lock);
    }
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
process_sleep(void)
{
  assert(spin_holding(&ptable.lock));

  my_process()->state = PROCESS_SLEEPING;
  scheduler_yield();
}

static void
process_wakeup(struct Process *proc)
{
  assert(spin_holding(&ptable.lock));

  proc->state = PROCESS_RUNNABLE;
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

    process_sleep();
  }

  spin_unlock(&ptable.lock);

  return r;
}

// A process' very first scheduling by scheduler() will switch here.
static void
process_run(void)
{
  // Still holding the process table lock.
  spin_unlock(&ptable.lock);

  // "Return" to the user space.
  process_pop_tf(my_process()->tf);
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
