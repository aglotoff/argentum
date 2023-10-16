#include <kernel.h>
#include <mmu.h>
#include <page.h>
#include <regs.h>
#include <vm.h>

//
// Implementation of architecture-specific VM functions.
//
// By design, ARMv7 MMU uses two translation tables. The kernel is located in
// the upper part of address space (above VIRT_KERNEL_BASE) and managed by the
// page table in TTBR1. User processes are in the lower part of memory (below
// VIRT_KERNEL_BASE) and managed by the page table in TTBR0. On each context
// switch, TTBR0 is updated to point to the page table of the current process.
// The value of TTBR1 should never change.
// 
// Since the ARM hardware support 1K page tables at the second level, but our
// kernel manages physical memory in units of 4K pages, we fit two second-level
// tables in one page (and use the remaining space to store extra flags that are
// not provided by the hardware for each page table entry).
//

#define L2_TABLES_PER_PAGE  2

static int *
arch_vm_pte_ext(void *pte)
{
  return (int *) ((l2_desc_t *) pte + (L2_NR_ENTRIES * L2_TABLES_PER_PAGE));
}

/**
 * Check whether a page table entry is valid.
 * 
 * @param pte Pointer to the page table entry
 * 
 * @return 1 if the page table entry is valid, 0 otherwise
 */
int
arch_vm_pte_valid(void *pte)
{
  // In our implementation, all valid PTEs map small pages 
  return (*(l2_desc_t *) pte & L2_DESC_TYPE_SM) == L2_DESC_TYPE_SM;
}

/**
 * Return the base physical address assotiated tith the given page table entry.
 * 
 * @param pte Pointer to the page table entry
 *
 * @return The corresponding base address
 */
physaddr_t
arch_vm_pte_addr(void *pte)
{
  return L2_DESC_SM_BASE(*(l2_desc_t *) pte);
}

/**
 * Return the mapping flags assotiated with the given page table entry.
 * 
 * @param pte Pointer to the page table entry
 *
 * @return The corresponding mapping flags
 */
int
arch_vm_pte_flags(void *pte)
{
  return *arch_vm_pte_ext(pte);
}

// Map VM flags to ARMv7 MMU AP bits
static int flags_to_ap[] = {
  [VM_READ]                      = AP_PRIV_RO, 
  [VM_WRITE]                     = AP_PRIV_RW, 
  [VM_READ | VM_WRITE]           = AP_PRIV_RW, 
  [VM_USER | VM_READ]            = AP_USER_RO, 
  [VM_USER | VM_WRITE]           = AP_BOTH_RW, 
  [VM_USER | VM_READ | VM_WRITE] = AP_BOTH_RW, 
};

/**
 * Set a page table entry.
 * 
 * @param pte   Pointer to the page table entry
 * @param pa    Base physical address
 * @param flags Mapping flags
 */
void
arch_vm_pte_set(void *pte, physaddr_t pa, int flags)
{
  int bits;

  bits = L2_DESC_AP(flags_to_ap[flags & 7]);
  if ((flags & VM_USER) && !(flags & VM_EXEC))
    bits |= L2_DESC_SM_XN;
  if (!(flags & VM_NOCACHE))
    bits |= (L2_DESC_B | L2_DESC_C);

  *(l2_desc_t *) pte = pa | bits | L2_DESC_TYPE_SM;
  *arch_vm_pte_ext(pte) = flags;
}

/**
 * Clear a page table entry.
 * 
 * @param pte Pointer to the page table entry
 */
void
arch_vm_pte_clear(void *pte)
{
  *(l2_desc_t *) pte = 0;
  *arch_vm_pte_ext(pte) = 0;
}

/**
 * Invalidate TLB entries matching the specified virtual address.
 *
 * @param va The virtual address of the page to invalidate
 */
void
arch_vm_invalidate(uintptr_t va)
{
  cp15_tlbimva(va);
}

/**
 * Get a page table entry for the given virtual address.
 * 
 * @param pgtab Pointer to the page table
 * @param va    The virtual address
 * @param alloc Whether to allocate memory for the relevant entry if it doesn't
 *              exist
 * 
 * @return Pointer to the page table entry for the specified virtual address
 *         or NULL if the relevant entry does not exist
 */
void *
arch_vm_lookup(void *pgtab, uintptr_t va, int alloc)
{
  l1_desc_t *tt, *tte;
  l2_desc_t *pte;

  tt = (l1_desc_t *) pgtab;
  tte = &tt[L1_IDX(va)];
  if ((*tte & L1_DESC_TYPE_MASK) == L1_DESC_TYPE_FAULT) {
    struct Page *page;
    physaddr_t pa;

    if (!alloc || (page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
  
    page->ref_count++;

    pa = page2pa(page);

    // Allocate space for two second-level page tables at a time
    tt[(L1_IDX(va) & ~1) + 0] = pa | L1_DESC_TYPE_TABLE;
    tt[(L1_IDX(va) & ~1) + 1] = (pa + L2_TABLE_SIZE) | L1_DESC_TYPE_TABLE;
  } else if ((*tte & L1_DESC_TYPE_MASK) != L1_DESC_TYPE_TABLE) {
    // The requested va belongs to a permanently mapped section
    panic("not a page table");
  }

  pte = PA2KVA(L1_DESC_TABLE_BASE(*tte));
  return &pte[L2_IDX(va)];
}

/**
 * Set a 1Mb section entry.
 * 
 * @param tte   Pointer to the section entry
 * @param pa    Base physical address
 * @param flags Mapping flags
 */
static inline void
arch_vm_section_set(l1_desc_t *tte, physaddr_t pa, int flags)
{
  int bits;

  bits = L1_DESC_SECT_AP(flags_to_ap[flags & 7]);
  if ((flags & VM_USER) && !(flags & VM_EXEC))
    bits |= L1_DESC_SECT_XN;
  if (!(flags & VM_NOCACHE))
    bits |= (L1_DESC_SECT_B | L1_DESC_SECT_C);

  *tte = pa | bits | L1_DESC_TYPE_SECT;
}

/**
 * Setup a permanent mapping for the given memory region in the master
 * translation table. The memory region must be page-aligned.
 * 
 * @param va    The starting virtual address
 * @param pa    The physical address to map to
 * @param n     The size of the memory region to be mapped (in bytes)
 * @param flags Mapping flags
 */
static void
arch_vm_fixed_map(uintptr_t va, uint32_t pa, size_t n, int flags)
{ 
  assert(va % PAGE_SIZE == 0);
  assert(pa % PAGE_SIZE == 0);
  assert(n  % PAGE_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce memory overhead for
    // second-level page tables
    if ((va % L1_SECTION_SIZE == 0) && (pa % L1_SECTION_SIZE == 0) &&
        (n  % L1_SECTION_SIZE == 0)) {
      l1_desc_t *tte = (l1_desc_t *) kernel_pgtab + L1_IDX(va);

      if (*tte)
        panic("TTE for %p already exists", va);

      arch_vm_section_set(tte, pa, flags);

      va += L1_SECTION_SIZE;
      pa += L1_SECTION_SIZE;
      n  -= L1_SECTION_SIZE;
    } else {
      l2_desc_t *pte = arch_vm_lookup(kernel_pgtab, va, 1);

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

/**
 * Setup the master translation table.
 */
void
arch_vm_init(void)
{
  struct Page *page;

  // Place pages mapped by 'entry_pgdir' to the free list
  page_free_region(PAGE_SIZE, PHYS_KERNEL_LOAD);
  page_free_region(KVA2PA(boot_alloc(0)), PHYS_ENTRY_LIMIT);

  // Allocate the master translation table
  if ((page = page_alloc_block(2, PAGE_ALLOC_ZERO)) == NULL)
    panic("cannot allocate kernel page table");

  kernel_pgtab = page2kva(page);
  page->ref_count++;

  // Map all physical memory at VIRT_KERNEL_BASE
  // Permissions: kernel RW, user NONE
  arch_vm_fixed_map(VIRT_KERNEL_BASE, 0, PHYS_LIMIT, VM_READ | VM_WRITE);

  // Map I/O devices
  // Permissions: kernel RW, user NONE, disable cache 
  arch_vm_fixed_map(VIRT_KERNEL_BASE + PHYS_LIMIT, PHYS_LIMIT,
                    VIRT_VECTOR_BASE - (VIRT_KERNEL_BASE + PHYS_LIMIT),
                    VM_READ | VM_WRITE | VM_NOCACHE);

  // Map exception vectors at VIRT_VECTOR_BASE
  // Permissions: kernel R, user NONE
  arch_vm_fixed_map(VIRT_VECTOR_BASE, 0, PAGE_SIZE, VM_READ);

  arch_vm_init_percpu();

  // Place the rest of the pages to the free list
  page_free_region(PHYS_ENTRY_LIMIT, PHYS_LIMIT);
}

/**
 * Switch from the minimal entry translation table to the full master
 * translation table
 */
void
arch_vm_init_percpu(void)
{
  cp15_ttbr0_set(KVA2PA(kernel_pgtab));
  cp15_ttbr1_set(KVA2PA(kernel_pgtab));

  cp15_ttbcr_set(1);  // TTBR0 table size is 8Kb

  cp15_tlbiall();
}

// Page block allocation order for user process page tables (8Kb)
#define PGTAB_ORDER 1

/**
 * Create a user process page table.
 *
 * @return Pointer to the allocated page table or NULL if out of memory
 */
void *
arch_vm_create(void)
{
  struct Page *page;

  if ((page = page_alloc_block(PGTAB_ORDER, PAGE_ALLOC_ZERO)) == NULL)
    return NULL;

  page->ref_count++;

  return page2kva(page);
}

/**
 * Destroy a user process page table.
 */
void
arch_vm_destroy(void *pgtab)
{
  struct Page *page;
  l1_desc_t *trtab;
  
  unsigned i, j;
  
  trtab = (l1_desc_t *) pgtab;

  // Free all allocated second-level page tables
  for (i = 0; i < L1_IDX(VIRT_KERNEL_BASE); i += L2_TABLES_PER_PAGE) {
    l2_desc_t *pt;

    if (!trtab[i])
      continue;

    page = pa2page(L2_DESC_SM_BASE(trtab[i]));
    pt   = (l2_desc_t *) page2kva(page);

    // Check that the caller has removed all mappings
    for (j = 0; j < L2_NR_ENTRIES * L2_TABLES_PER_PAGE; j++)
      if (arch_vm_pte_valid(&pt[j]))
        panic("pte still in use");

    if (--page->ref_count == 0)
      page_free_one(page);
  }

  // Finally, free the first-level translation table itself
  page = kva2page(trtab);
  if (--page->ref_count == 0)
      page_free_block(page, PGTAB_ORDER);
}

/**
 * Load a page table (architecture-specific version).
 *
 * @param pgtab Pointer to the page table to be loaded.
 */
void
arch_vm_load(void *pgtab)
{
  cp15_ttbr0_set(KVA2PA(pgtab));
  cp15_tlbiall();
}
