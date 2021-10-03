#include <assert.h>
#include <elf.h>
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
int nprocesses;

static struct KObjectCache *process_cache;

static struct {
  struct ListLink runqueue;
  struct Spinlock lock;
} sched;

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
  process_cache = kobject_cache_create("process_cache", sizeof(struct Process),
                                       NULL, NULL);
  if (process_cache == NULL)
    panic("cannot allocate process_cache");

  list_init(&sched.runqueue);

  spin_init(&sched.lock, "sched");
}

struct Process *
process_alloc(void)
{
  extern void trapret(void);

  struct PageInfo *page;
  struct Process *process;
  uint8_t *sp;

  process = (struct Process *) kobject_alloc(process_cache);

  // Allocate per-process kernel stack
  if ((page = page_alloc(0)) == NULL) {
    kobject_free(process_cache, process);
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
region_alloc(struct Process *p, uintptr_t va, size_t n)
{
  struct PageInfo *page;
  uintptr_t a, start, end;

  start = va & ~(PAGE_SIZE - 1);
  end   = (va + n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  for (a = start; a < end; a += PAGE_SIZE) {
    if ((page = page_alloc(0)) == NULL)
      panic("out of memory");
    if ((vm_insert_page(p->trtab, page, (void *) a, AP_BOTH_RW)) != 0)
      panic("out of memory");
  }
}

void
process_create(void *binary)
{
  static int next_pid;
  
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;
  struct Process *process;

  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0)
    panic("Invalid ELF binary");
  
  if ((process = process_alloc()) == NULL)
    panic("out of memory");

  process_setup_vm(process);

  vm_switch_user(process->trtab);

  ph = (Elf32_Phdr *) ((uint8_t *) elf + elf->phoff);
  eph = ph + elf->phnum;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;
    if (ph->filesz > ph->memsz)
      panic("load_icode: invalid segment header");
    
    region_alloc(process, ph->vaddr, ph->memsz);

    memmove((void *) ph->vaddr, binary + ph->offset, ph->filesz);
    if (ph->filesz < ph->memsz)
      memset((uint8_t *) ph->vaddr + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  region_alloc(process, USTACK_TOP - USTACK_SIZE, USTACK_SIZE);

  vm_switch_kernel();

  process->tf->sp_usr = USTACK_TOP;
  process->tf->psr = PSR_M_USR | PSR_F;
  process->tf->pc = elf->entry;

  process->pid = ++next_pid;
  nprocesses++;

  spin_lock(&sched.lock);
  list_add_back(&sched.runqueue, &process->link);
  spin_unlock(&sched.lock);
}

void
process_destroy(void)
{
  nprocesses--;

  spin_lock(&sched.lock);
  context_switch(&mycpu()->process->context, mycpu()->scheduler);
}

void
scheduler(void)
{
  struct ListLink *link;
  struct Process *process;
  
  spin_lock(&sched.lock);

  for (;;) {
    if (!list_empty(&sched.runqueue)) {
      link = sched.runqueue.next;
      list_remove(link);

      process = LIST_CONTAINER(link, struct Process, link);
      mycpu()->process = process;

      vm_switch_user(process->trtab);
      context_switch(&mycpu()->scheduler, process->context);
    } else {
      // If there are no environments in the system, drop into the kernel
      // monitor.
      if (nprocesses == 0) {
        cprintf("No runnable processes in the system!\n");
        for (;;)
          monitor(NULL);
      }

      // Mark that no process is running on this CPU.
      mycpu()->process = NULL;
      vm_switch_kernel();

      spin_unlock(&sched.lock);
      spin_lock(&sched.lock);
    }
  }
}

void
process_yield(void)
{
  spin_lock(&sched.lock);

  list_add_back(&sched.runqueue, &mycpu()->process->link);

  context_switch(&mycpu()->process->context, mycpu()->scheduler);

  spin_unlock(&sched.lock);
}

// A process' very first scheduling by scheduler() will switch here.
static void
process_run(void)
{
  // Still holding the scheduler lock.
  spin_unlock(&sched.lock);

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
