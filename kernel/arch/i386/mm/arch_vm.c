#include <sys/mman.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/page.h>

#include <arch/i386/mmu.h>

#define MAKE_ENTRY_PDE(pa)  ((pa) | PDE_PS | PDE_W | PDE_P)

// Initial translation table to "get off the ground"
__attribute__((__aligned__(PGDIR_SIZE))) pde_t
entry_pgdir[PGDIR_NR_ENTRIES] = {
  // Identity mapping for the first 4MB of physical memory (just enough to
  // load the entry point code):
  [0x0] = MAKE_ENTRY_PDE(0x0),

  // Higher-half mappings for the first 4MB of physical memory (should be
  // enough to initialize the page allocator, and setup the master page
  // directory):
  [PGDIR_IDX(VIRT_KERNEL_BASE) + 0x0] = MAKE_ENTRY_PDE(0x0),
};

void
arch_vm_load(void *pgtab)
{
  // TODO
  (void) pgtab;
}

void
arch_vm_load_kernel(void)
{
  // TODO
}

int
arch_vm_pte_valid(void *pte)
{
  // TODO
  (void) pte;
  return -1;
}

physaddr_t
arch_vm_pte_addr(void *pte)
{
  // TODO
  (void) pte;
  return 0;
}

int
arch_vm_pte_flags(void *pte)
{
  // TODO
  (void) pte;
  return -1;
}

void
arch_vm_pte_set(void *pte, physaddr_t pa, int flags)
{
  // TODO
  (void) pte;
  (void) pa;
  (void) flags;
}

void
arch_vm_pte_clear(void *pte)
{
  // TODO
  (void) pte;
}

void
arch_vm_invalidate(uintptr_t va)
{
  // TODO
  (void) va;
}

void *
arch_vm_lookup(void *pgtab, uintptr_t va, int alloc)
{
  // TODO
  (void) pgtab;
  (void) va;
  (void) alloc;
  return NULL;
}

void
arch_vm_init(void)
{
  // TODO
}

void
arch_vm_init_percpu(void)
{
  // TODO
}

void *
arch_vm_create(void)
{
  // TODO
  return NULL;
}

void
arch_vm_destroy(void *pgtab)
{
  // TODO
  (void) pgtab;
}
