#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <string.h>

#include "armv7.h"
#include "console.h"
#include "kobject.h"
#include "mmu.h"
#include "monitor.h"
#include "page.h"
#include "process.h"
#include "spinlock.h"
#include "trap.h"
#include "vm.h"

struct Cpu cpus[NCPU];

static struct KObjectPool *process_pool;

#define NBUCKET   256

static struct {
  int             nprocesses;
  struct ListLink hash[NBUCKET];
  struct ListLink runqueue;
  struct Spinlock lock;
} ptable;

static void process_run(void);
static void process_pop_tf(struct Trapframe *);

int
cpuid(void)
{
  return read_mpidr() & 3;
}

struct Cpu *
mycpu(void)
{
  assert((read_cpsr() & PSR_I) && (read_cpsr() & PSR_F));
  return &cpus[cpuid()];
}

struct Process *
myprocess(void)
{
  struct Cpu *cpu;
  struct Process *process;
  
  irq_save();

  cpu = mycpu();
  process = cpu->process;

  irq_restore();

  return process;
}

void
process_init(void)
{
  struct ListLink *link;

  process_pool = kobject_pool_create("process_pool",
                                       sizeof(struct Process), 0);
  if (process_pool == NULL)
    panic("cannot allocate process_pool");

  for (link = ptable.hash; link < & ptable.hash[NBUCKET]; link++)
    list_init(link);
  list_init(&ptable.runqueue);
  spin_init(&ptable.lock, "ptable");
}

struct Process *
process_alloc(void)
{
  extern void trapret(void);

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
  process->pid = ++next_pid;
  list_add_back(&ptable.hash[process->pid % NBUCKET], &process->pid_link);
  ptable.nprocesses++;
  spin_unlock(&ptable.lock);

  cprintf("\x1b[1;32m[%08x]\x1b[0m spawn process %08x\n",
          myprocess() ? myprocess()->pid : 0,
          process->pid);

  return process;
}

void
process_setup_vm(struct Process *p)
{
  struct PageInfo *page;

  if ((page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL)
    panic("out of memory");

  p->trtab = (tte_t *) page2kva(page);
  page->ref_count++;
}

static void
process_load_binary(struct Process *proc, const void *binary)
{
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;
  
  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0)
    panic("Invalid ELF binary");

  ph = (Elf32_Phdr *) ((uint8_t *) elf + elf->phoff);
  eph = ph + elf->phnum;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;

    if (ph->filesz > ph->memsz)
      panic("invalid segment header");
    
    if (vm_alloc_region(proc->trtab, (void *) ph->vaddr, ph->memsz) < 0)
      panic("vm_alloc_region");

    if (vm_copy_out(proc->trtab, (void *) ph->vaddr, binary + ph->offset,
                    ph->filesz) < 0)
      panic("vm_copy_out");

    proc->size = ph->vaddr + ph->memsz;
  }

  // Allocate user stack.
  if (vm_alloc_region(proc->trtab, (void *) (USTACK_TOP - USTACK_SIZE),
                      USTACK_SIZE) < 0)
    panic("vm_alloc_region");

  proc->tf->r0     = 0;                   // argc
  proc->tf->r1     = 0;                   // argv
  proc->tf->sp_usr = USTACK_TOP;          // stack pointer
  proc->tf->psr    = PSR_M_USR | PSR_F;   // user mode, interrupts enabled
  proc->tf->pc     = elf->entry;          // process entry point
}

void
process_create(const void *binary)
{
  struct Process *proc;

  if ((proc = process_alloc()) == NULL)
    panic("out of memory");

  process_setup_vm(proc);

  process_load_binary(proc, binary);

  spin_lock(&ptable.lock);
  proc->state = PROCESS_RUNNABLE;
  list_add_back(&ptable.runqueue, &proc->link);
  spin_unlock(&ptable.lock);
}

void
process_free(struct Process *proc)
{
  struct PageInfo *page;

  page = kva2page(proc->kstack);
  if (--page->ref_count == 0)
    page_free(page);

  spin_lock(&ptable.lock);
  list_remove(&proc->pid_link);
  spin_unlock(&ptable.lock);

  kobject_free(process_pool, proc);
}

void
process_destroy(int status)
{
  struct Process *proc = myprocess();
  
  if (status == 0)
    cprintf("\x1b[1;32m[%08x]\x1b[0m exit with code %d\n",
            proc->pid, status);
  else
    cprintf("\x1b[1;31m[%08x]\x1b[0m exit with code %d\n",
            proc->pid, status);

  vm_free(proc->trtab);

  spin_lock(&ptable.lock);
  proc->state = PROCESS_ZOMBIE;
  ptable.nprocesses--;
  context_switch(&mycpu()->process->context, mycpu()->scheduler);
}

pid_t
process_fork(void)
{
  struct Process *proc, *myproc = myprocess();

  if ((proc = process_alloc()) == NULL)
    return -ENOMEM;

  if ((proc->trtab = vm_clone(myproc->trtab)) == NULL) {
    process_free(proc);
    return -ENOMEM;
  }

  proc->size  = myproc->size;
  proc->parent = myproc;

  *proc->tf = *myproc->tf;
  proc->tf->r0 = 0;

  spin_lock(&ptable.lock);
  proc->state = PROCESS_RUNNABLE;
  list_add_back(&myproc->children, &proc->sibling);
  list_add_back(&ptable.runqueue, &proc->link);
  spin_unlock(&ptable.lock);

  return proc->pid;
}

void
scheduler(void)
{
  struct ListLink *link;
  struct Process *process;
  
  spin_lock(&ptable.lock);

  for (;;) {
    if (!list_empty(&ptable.runqueue)) {
      link = ptable.runqueue.next;
      list_remove(link);

      process = LIST_CONTAINER(link, struct Process, link);
      assert(process->state == PROCESS_RUNNABLE);

      process->state = PROCESS_RUNNING;
      mycpu()->process = process;

      vm_switch_user(process->trtab);
      context_switch(&mycpu()->scheduler, process->context);
    } else {
      // If there are no more proceses to run, drop into the kernel monitor.
      if (ptable.nprocesses == 0) {
        cprintf("No runnable processes in the system!\n");
        for (;;)
          monitor(NULL);
      }

      // Mark that no process is running on this CPU.
      mycpu()->process = NULL;
      vm_switch_kernel();

      spin_unlock(&ptable.lock);
      spin_lock(&ptable.lock);
    }
  }
}

void
process_yield(void)
{
  spin_lock(&ptable.lock);

  mycpu()->process->state = PROCESS_RUNNABLE;
  list_add_back(&ptable.runqueue, &mycpu()->process->link);
  context_switch(&mycpu()->process->context, mycpu()->scheduler);

  spin_unlock(&ptable.lock);
}

// A process' very first scheduling by scheduler() will switch here.
static void
process_run(void)
{
  // Still holding the process table lock.
  spin_unlock(&ptable.lock);

  // "Return" to the user space.
  process_pop_tf(myprocess()->tf);
}

// Restores the register values in the Trapframe.
// This function doesn't return.
static void
process_pop_tf(struct Trapframe *tf)
{
  __asm__ __volatile__(
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
