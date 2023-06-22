#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>

static void mmu_map_static(l1_desc_t *, uintptr_t, uint32_t, size_t, int);

/*
 * ----------------------------------------------------------------------------
 * Translation Table Initializaion
 * ----------------------------------------------------------------------------
 */

// Master kernel translation table.
static l1_desc_t *kern_pgdir;

void
mmu_init(void)
{
  extern uint8_t _start[];

  struct Page *page;

  // Allocate the master translation table
  if ((page = page_alloc_block(2, PAGE_ALLOC_ZERO)) == NULL)
    panic("out of memory");

  kern_pgdir = (l1_desc_t *) page2kva(page);

  // Map all physical memory at VIRT_KERNEL_BASE
  // Permissions: kernel RW, user NONE
  mmu_map_static(kern_pgdir, VIRT_KERNEL_BASE, 0, PHYS_LIMIT, VM_READ | VM_WRITE);

  // Map all devices 
  mmu_map_static(kern_pgdir, VIRT_KERNEL_BASE + PHYS_LIMIT, PHYS_LIMIT,
              VIRT_VECTOR_BASE - VIRT_KERNEL_BASE - PHYS_LIMIT,
              VM_READ | VM_WRITE | VM_NOCACHE);

  // Map exception vectors at VIRT_VECTOR_BASE
  // Permissions: kernel R, user NONE
  mmu_map_static(kern_pgdir, VIRT_VECTOR_BASE, (uint32_t) _start, PAGE_SIZE, VM_READ);

  mmu_init_percpu();
}

void
mmu_init_percpu(void)
{
  // Switch from the minimal entry translation table to the full translation
  // table.
  cp15_ttbr0_set(KVA2PA(kern_pgdir));
  cp15_ttbr1_set(KVA2PA(kern_pgdir));

  // Size of the TTBR0 translation table is 8KB.
  cp15_ttbcr_set(1);

  cp15_tlbiall();
}

/*
 * ----------------------------------------------------------------------------
 * Low-Level Operations
 * ----------------------------------------------------------------------------
 */

// VM proptection flags to ARM MMU AP bits
static int prot_to_ap[] = {
  [VM_READ]                      = AP_PRIV_RO, 
  [VM_WRITE]                     = AP_PRIV_RW, 
  [VM_READ | VM_WRITE]           = AP_PRIV_RW, 
  [VM_USER | VM_READ]            = AP_USER_RO, 
  [VM_USER | VM_WRITE]           = AP_BOTH_RW, 
  [VM_USER | VM_READ | VM_WRITE] = AP_BOTH_RW, 
};

void
mmu_pte_set(l2_desc_t *pte, physaddr_t pa, int prot)
{
  int flags;

  flags = L2_DESC_AP(prot_to_ap[prot & 7]);
  if ((prot & VM_USER) && !(prot & VM_EXEC))
    flags |= L2_DESC_SM_XN;
  if (!(prot & VM_NOCACHE))
    flags |= (L2_DESC_B | L2_DESC_C);

  *pte = pa | flags | L2_DESC_TYPE_SM;
  mmu_pte_set_flags(pte, prot);
}

static inline void
mmu_l1_set_pgtab(l1_desc_t *tte, physaddr_t pa)
{
  *tte = pa | L1_DESC_TYPE_TABLE;
}

static inline void
mmu_l1_set_sect(l1_desc_t *tte, physaddr_t pa, int prot)
{
  int flags;

  flags = L1_DESC_SECT_AP(prot_to_ap[prot & 7]);
  if ((prot & VM_USER) && !(prot & VM_EXEC))
    flags |= L1_DESC_SECT_XN;
  if (!(prot & VM_NOCACHE))
    flags |= (L1_DESC_SECT_B | L1_DESC_SECT_C);

  *tte = pa | flags | L1_DESC_TYPE_SECT;
}


//
// Returns the pointer to the page table entry in the translation table pgdir
// that corresponds to the virtual address va. If alloc is not zero, allocate
// any required page tables.
//
l2_desc_t *
mmu_pte_get(l1_desc_t *pgdir, uintptr_t va, int alloc)
{
  l1_desc_t *tte;
  l2_desc_t *pgtab;

  tte = &pgdir[L1_IDX(va)];
  if ((*tte & L1_DESC_TYPE_MASK) == L1_DESC_TYPE_FAULT) {
    struct Page *page;

    if (!alloc || (page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
    
    page->ref_count++;

    // We could fit four page tables into one physical page. But because we
    // also need to associate some metadata with each entry, we store only two
    // page tables in the bottom of the allocated physical page.
    mmu_l1_set_pgtab(&pgdir[(L1_IDX(va) & ~1) + 0], page2pa(page));
    mmu_l1_set_pgtab(&pgdir[(L1_IDX(va) & ~1) + 1], page2pa(page) + L2_TABLE_SIZE);
  } else if ((*tte & L1_DESC_TYPE_MASK) != L1_DESC_TYPE_TABLE) {
    panic("not a page table");
  }

  pgtab = PA2KVA(L1_DESC_TABLE_BASE(*tte));
  return &pgtab[L2_IDX(va)];
}

l1_desc_t *
mmu_pgtab_create(void)
{
  struct Page *page;

  if ((page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL)
    return NULL;

  page->ref_count++;

  return (l1_desc_t *) page2kva(page);
}

void
mmu_pgtab_destroy(l1_desc_t *pgtab)
{
  struct Page *page;
  l2_desc_t *pte;
  unsigned i;
  unsigned j;
  
  for (i = 0; i < L1_IDX(VIRT_KERNEL_BASE); i += 2) {
    if (!pgtab[i])
      continue;

    page = pa2page(L2_DESC_SM_BASE(pgtab[i]));

    pte = (l2_desc_t *) page2kva(page);
    for (j = 0; j < L2_NR_ENTRIES * 2; j++)
      assert(pte[j] == 0);

    if (--page->ref_count == 0)
      page_free_one(page);
  }

  page = kva2page(pgtab);
  if (--page->ref_count == 0)
      page_free_one(page);
}

// 
// Map [va, va + n) of virtual address space to physical [pa, pa + n) in
// the translation page pgdir.
//
// This function is only intended to set up static mappings in the kernel's
// portion of address space.
//
static void
mmu_map_static(l1_desc_t *pgdir, uintptr_t va, uint32_t pa, size_t n, int prot)
{ 
  assert(va % PAGE_SIZE == 0);
  assert(pa % PAGE_SIZE == 0);
  assert(n  % PAGE_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce the memory
    // management overhead.
    if ((va % L1_SECTION_SIZE == 0) &&
        (pa % L1_SECTION_SIZE == 0) &&
        (n  % L1_SECTION_SIZE == 0)) {
      l1_desc_t *tte;

      tte = &pgdir[L1_IDX(va)];
      if (*tte)
        panic("remap");

      mmu_l1_set_sect(tte, pa, prot);

      va += L1_SECTION_SIZE;
      pa += L1_SECTION_SIZE;
      n  -= L1_SECTION_SIZE;
    } else {
      l2_desc_t *pte;

      if ((pte = mmu_pte_get(pgdir, va, 1)) == NULL)
        panic("out of memory");
      if (*pte)
        panic("remap %p", *pte);

      mmu_pte_set(pte, pa, prot);

      va += PAGE_SIZE;
      pa += PAGE_SIZE;
      n  -= PAGE_SIZE;
    }
  }
}

/*
 * ----------------------------------------------------------------------------
 * Translation Table Switch
 * ----------------------------------------------------------------------------
 */

/**
 * Load the master kernel translation table.
 */
void
mmu_switch_kernel(void)
{
  cp15_ttbr0_set(KVA2PA(kern_pgdir));
  cp15_tlbiall();
}

/**
 * Load the user process translation table.
 * 
 * @param pgdir Pointer to the translation table to be loaded.
 */
void
mmu_switch_user(l1_desc_t *pgdir)
{
  cp15_ttbr0_set(KVA2PA(pgdir));
  cp15_tlbiall();
}

struct Page *
vm_page_lookup(l1_desc_t *pgdir, const void *va, l2_desc_t **pte_store)
{
  l2_desc_t *pte;

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
    panic("bad va: %p", va);

  pte = mmu_pte_get(pgdir, (uintptr_t ) va, 0);

  if (pte_store)
    *pte_store = pte;

  if ((pte == NULL) || !mmu_pte_valid(pte))
    return NULL;

  return pa2page(mmu_pte_base(pte));
}

int
vm_page_insert(l1_desc_t *pgdir, struct Page *page, void *va, unsigned perm)
{
  l2_desc_t *pte;

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
    panic("bad va: %p", va);

  if ((pte = mmu_pte_get(pgdir, (uintptr_t ) va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions.
  page->ref_count++;

  // If present, remove the previous mapping.
  vm_page_remove(pgdir, va);

  mmu_pte_set(pte, page2pa(page), perm);

  return 0;
}

void
vm_page_remove(l1_desc_t *pgdir, void *va)
{
  struct Page *page;
  l2_desc_t *pte;

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
    panic("bad va: %p", va);

  if ((page = vm_page_lookup(pgdir, va, &pte)) == NULL)
    return;

  if (--page->ref_count == 0)
    page_free_one(page);

  mmu_pte_clear(pte);
  mmu_invalidate_va(va);
}
