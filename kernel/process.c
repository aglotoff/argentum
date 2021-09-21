#include <assert.h>
#include <elf.h>
#include <string.h>

#include "armv7.h"
#include "console.h"
#include "mmu.h"
#include "page.h"
#include "process.h"
#include "spinlock.h"
#include "trap.h"
#include "vm.h"

struct Cpu cpus[NCPU];

struct Process initproc;

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
  int flags;
  
  flags = irq_save();

  cpu = mycpu();
  process = cpu->process;

  irq_restore(flags);

  return process;
}

void
process_init(struct Process *p)
{
  extern void trapret(void);

  struct PageInfo *page;
  uint8_t *sp;

  if ((page = page_alloc(0, 0)) == NULL)
    panic("out of memory");

  p->kstack = (uint8_t *) page2kva(page);
  page->ref_count++;

  sp = p->kstack + PAGE_SIZE;

  sp -= sizeof *p->tf;
  p->tf = (struct Trapframe *) sp;
  p->tf->sp_usr = USTACK_TOP;
  p->tf->psr = PSR_M_USR | PSR_F;

  sp -= sizeof *p->context;
  p->context = (struct Context *) sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->lr = (uint32_t) trapret;
}

void
process_setup_vm(struct Process *p)
{
  struct PageInfo *page;

  if ((page = page_alloc(1, PAGE_ALLOC_ZERO)) == NULL)
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
    if ((page = page_alloc(0, 0)) == NULL)
      panic("out of memory");
    if ((vm_insert_page(p->trtab, page, (void *) a, AP_BOTH_RW)) != 0)
      panic("out of memory");
  }
}

void
process_create(void *binary)
{
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;

  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0)
    panic("Invalid ELF binary");
  
  process_init(&initproc);
  process_setup_vm(&initproc);

  vm_switch_user(initproc.trtab);

  ph = (Elf32_Phdr *) ((uint8_t *) elf + elf->phoff);
  eph = ph + elf->phnum;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;
    if (ph->filesz > ph->memsz)
      panic("load_icode: invalid segment header");
    
    region_alloc(&initproc, ph->vaddr, ph->memsz);

    memmove((void *) ph->vaddr, binary + ph->offset, ph->filesz);
    if (ph->filesz < ph->memsz)
      memset((uint8_t *) ph->vaddr + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  region_alloc(&initproc, USTACK_TOP - USTACK_SIZE, USTACK_SIZE);

  vm_switch_kernel();

  initproc.tf->pc = elf->entry;
}

void
process_run(void)
{
  int flags;

  flags = irq_save();

  mycpu()->process = &initproc;
  vm_switch_user(initproc.trtab);
  context_switch(&mycpu()->scheduler, initproc.context);

  irq_restore(flags);
}
