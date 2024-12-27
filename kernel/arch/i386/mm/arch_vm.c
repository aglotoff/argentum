#include <sys/mman.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/page.h>

#include <arch/i386/mmu.h>
#include <arch/i386/regs.h>

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

// Master kernel page directory
static void *kernel_pgdir;

void
arch_vm_load(void *pgdir)
{
  cr3_set(KVA2PA(pgdir));
}

void
arch_vm_load_kernel(void)
{
  arch_vm_load(kernel_pgdir);
}

int
arch_vm_pte_valid(void *pte)
{
  return *((pte_t *) pte) & PTE_P;
}

physaddr_t
arch_vm_pte_addr(void *pte)
{
  return PTE_BASE(*(pte_t *) pte);
}

int
arch_vm_pte_flags(void *pte)
{
  int bits = PTE_FLAGS(*(pte_t *) pte);
  int flags = VM_NONE;
  
  if (bits & PTE_P) flags |= (VM_EXEC | VM_READ);
  if (bits & PTE_W) flags |= VM_WRITE;
  if (bits & PTE_U) flags |= VM_USER;
  if (bits & PTE_PCD) flags |= VM_NOCACHE;

  if (bits & PTE_AVAIL_COW) flags |= VM_COW;
  if (bits & PTE_AVAIL_PAGE) flags |= VM_PAGE;

  return flags;
}

void
arch_vm_pte_set(void *pte, physaddr_t pa, int flags)
{
  int bits = PTE_P;

  if (flags & VM_WRITE) bits |= PTE_W;
  if (flags & VM_USER) bits |= PTE_U;
  if (flags & VM_NOCACHE) bits |= PTE_PCD;

  if (flags & VM_COW) bits |= PTE_AVAIL_COW;
  if (flags & VM_PAGE) bits |= PTE_AVAIL_PAGE;

  *((pte_t *) pte) = pa | bits;
}

void
arch_vm_pte_clear(void *pte)
{
  *((pte_t *) pte) = 0;
}

void
arch_vm_invalidate(uintptr_t va)
{
  asm volatile("invlpg (%0)" : : "r" (va) : "memory");
}

void *
arch_vm_lookup(void *pgtab, uintptr_t va, int alloc)
{
  pde_t *pgdir, *pde;
  pte_t *pte;

  // Make sure the user and the kernel mappings are modified only in the
  // corresponding page tables
  if ((va >= VIRT_KERNEL_BASE) && (pgtab != kernel_pgdir))
    panic("user va %p must be below VIRT_KERNEL_BASE", va);
  if ((va < VIRT_KERNEL_BASE) && (pgtab == kernel_pgdir))
    panic("kernel va %p must be above VIRT_KERNEL_BASE", va);

  pgdir = (pde_t *) pgtab;
  pde = &pgdir[PGDIR_IDX(va)];
  if (!(*pde & PDE_P)) {
    struct Page *page;
    physaddr_t pa;

    if (!alloc || (page = page_alloc_one(PAGE_ALLOC_ZERO, PAGE_SIZE)) == NULL)
      return NULL;
  
    page->ref_count++;

    pa = page2pa(page);

    *pde = pa | PTE_U | PTE_W | PTE_P;
  } else if (*pte & PDE_PS) {
    // trying to remap a fixed section
    panic("not a page table");
  }

  pte = PA2KVA(PDE_BASE(*pde));
  return &pte[PGTAB_IDX(va)];
}

static inline void
init_large_desc(pde_t *pde, physaddr_t pa, int flags)
{
  int bits = PDE_P | PDE_PS;

  if (flags & VM_WRITE) bits |= PDE_W;
  if (flags & VM_USER) bits |= PDE_U;
  if (flags & VM_NOCACHE) bits |= PDE_PCD;

  *pde = pa | bits;
}

static void
init_fixed_mapping(uintptr_t va, uint32_t pa, size_t n, int flags)
{ 
  assert(va % PAGE_SIZE == 0);
  assert(pa % PAGE_SIZE == 0);
  assert(n  % PAGE_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce memory overhead for
    // second-level page tables
    if ((va % LARGE_PAGE_SIZE == 0) && (pa % LARGE_PAGE_SIZE == 0) &&
        (n  % LARGE_PAGE_SIZE == 0)) {
      pde_t *pde = (pde_t *) kernel_pgdir + PGDIR_IDX(va);

      if (*pde)
        panic("pde for %p already exists", va);

      init_large_desc(pde, pa, flags);

      va += LARGE_PAGE_SIZE;
      pa += LARGE_PAGE_SIZE;
      n  -= LARGE_PAGE_SIZE;
    } else {
      pte_t *pte = arch_vm_lookup(kernel_pgdir, va, 1);

      if (pte == NULL)
        panic("cannot allocate PTE for %p", va);
      if (arch_vm_pte_valid(pte))
        panic("PTE for %p already exists", va);

      arch_vm_pte_set(pte, pa, flags);

      va += PAGE_SIZE;
      pa += PAGE_SIZE;
      n  -= PAGE_SIZE;
    }
  }
}

void
arch_vm_init(void)
{
  // extern uint8_t _start[];

  struct Page *page;

  // Allocate the master translation table
  if ((page = page_alloc_block(2, PAGE_ALLOC_ZERO, PAGE_TAG_KERNEL_VM)) == NULL)
    panic("cannot allocate kernel page table");

  kernel_pgdir = page2kva(page);
  page->ref_count++;

  // Map all physical memory at VIRT_KERNEL_BASE
  // Permissions: kernel RW, user NONE
  init_fixed_mapping(VIRT_KERNEL_BASE, 0, PHYS_LIMIT, PROT_READ | PROT_WRITE);

  arch_vm_init_percpu();
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
