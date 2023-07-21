#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>

// TODO: move these into a seperate header
static void         arch_vm_create_kernel(void);
static void         arch_vm_init_percpu(void);
static int          arch_vm_pte_valid(void *);
static physaddr_t   arch_vm_pte_addr(void *);
static int          arch_vm_pte_flags(void *);
static void         arch_vm_pte_set(void *, physaddr_t, int);
static void         arch_vm_pte_clear(void *);
static void        *arch_vm_lookup(void *, uintptr_t, int);
static void        *arch_vm_create(void);
static void         arch_vm_destroy(void *);
static void         arch_vm_load(void *);
static void         arch_vm_invalidate(uintptr_t);

// Master kernel page table.
static void *kernel_pgtab;

/**
 * Initialize MMU, create and load the master page table. This function must be
 * called only on the bootstrap processor.
 */
void
vm_init(void)
{
  arch_vm_create_kernel();
  arch_vm_init_percpu();
}

/**
 * Initialize MMU and load the master page table on the current processor.
 */
void
vm_init_percpu(void)
{
  arch_vm_init_percpu();
}

/**
 * Allocate and initialize a new page table.
 * 
 * @return Pointer to the new page table or NULL if out of memory.
 */
void *
vm_create(void)
{
  return arch_vm_create();
}

/**
 * Destroy a page table.
 * 
 * The caller must remove all mappings from the page table before calling
 * this function.
 * 
 * @param pgtab Pointer to the page table to be destroyed.
 */
void
vm_destroy(void *pgtab)
{
  arch_vm_destroy(pgtab);
}

/**
 * Load the master kernel page table.
 */
void
vm_load_kernel(void)
{
  arch_vm_load(kernel_pgtab);
}

/**
 * Load a page table.
 *
 * @param pgtab Pointer to the page table to be loaded.
 */
void
vm_load(void *pgtab)
{
  arch_vm_load(pgtab);
}

/**
 * Check that mappings for the provided virtual address could be modified
 * in the specified page table.
 * 
 * @param pgtab Pointer to the page table
 * @param va    The virtual address
 */
static void
vm_assert(void *pgtab, uintptr_t va)
{
  if ((va >= VIRT_KERNEL_BASE) && (pgtab != kernel_pgtab))
    panic("user va %p must be below VIRT_KERNEL_BASE", va);
}

/**
 * Find a physical page mapped at the given virtual address.
 * 
 * @param pgtab       Pointer to the page table to search
 * @param va          The virtual address to search for
 * @param flags_store Pointer to the memory location to store the mapping flags
 *
 * @return Pointer to the page or NULL if there is no page mapped at the given
 *         address
 */
struct Page *
vm_page_lookup(void *pgtab, uintptr_t va, int *flags_store)
{
  void *pte;

  vm_assert(pgtab, va);

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return NULL;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & __VM_PAGE))
    return NULL;

  if (flags_store)
    *flags_store = arch_vm_pte_flags(pte);

  return pa2page(arch_vm_pte_addr(pte));
}

/**
 * Map a physical page at the given virtual address. If there is already a page
 * mapped at this address, remove it.
 * 
 * @param pgtab Pointer to the page table
 * @param page  Pointer to the page to be mapped
 * @param va    The virtual address
 * @param flags The mapping flags
 *
 * @retval 0       Success
 * @retval -ENOMEM Out of memory
 */
int
vm_page_insert(void *pgtab, struct Page *page, uintptr_t va, int flags)
{
  void *pte;

  vm_assert(pgtab, va);

  if ((pte = arch_vm_lookup(pgtab, va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions
  page->ref_count++;

  // If present, remove the previous mapping
  vm_page_remove(pgtab, (uintptr_t) va);

  arch_vm_pte_set(pte, page2pa(page), flags | __VM_PAGE);

  return 0;
}

/**
 * Unmap the physical page at the given virtual address. If there is no page
 * mapped at this address, do nothing.
 * 
 * @param pgtab The page table
 * @param va    The virtual address
 *
 * @return 0 on success
 */
int
vm_page_remove(void *pgtab, uintptr_t va)
{
  struct Page *page;
  void *pte;

  vm_assert(pgtab, va);

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return 0;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & __VM_PAGE))
    return 0;

  page = pa2page(arch_vm_pte_addr(pte));

  if (--page->ref_count == 0)
    page_free_one(page);

  arch_vm_pte_clear(pte);
  arch_vm_invalidate(va);

  return 0;
}

// -----------------------------------------------------------------------------
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
// TODO: move these into a separate file
//
// -----------------------------------------------------------------------------

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
static int
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
static physaddr_t
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
static int
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
static void
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
static void
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
static void
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
static void *
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
static void
arch_vm_create_kernel(void)
{
  extern uint8_t _start[];

  struct Page *page;

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
  arch_vm_fixed_map(VIRT_VECTOR_BASE, (physaddr_t) _start, PAGE_SIZE, VM_READ);
}

/**
 * Switch from the minimal entry translation table to the full master
 * translation table
 */
static void
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
static void *
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
static void
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
static void
arch_vm_load(void *pgtab)
{
  cp15_ttbr0_set(KVA2PA(pgtab));
  cp15_tlbiall();
}
