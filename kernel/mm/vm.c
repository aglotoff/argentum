#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>

static void        *arch_vm_create_kernel(void);
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
 * Initialize and load the master page table.
 */
void
vm_init(void)
{
  if ((kernel_pgtab = arch_vm_create_kernel()) == NULL)
    panic("out of memory");

  vm_init_percpu();
}

/**
 * Load the master page table.
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

  if ((va >= VIRT_KERNEL_BASE) && (pgtab != kernel_pgtab))
    panic("va must be below VIRT_KERNEL_BASE");

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return NULL;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & VM_ANONYMOUS))
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

  if ((va >= VIRT_KERNEL_BASE) && (pgtab != kernel_pgtab))
    panic("va must be below VIRT_KERNEL_BASE");

  if ((pte = arch_vm_lookup(pgtab, va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions
  page->ref_count++;

  // If present, remove the previous mapping
  vm_page_remove(pgtab, (uintptr_t) va);

  arch_vm_pte_set(pte, page2pa(page), flags | VM_ANONYMOUS);

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

  if ((va >= VIRT_KERNEL_BASE) && (pgtab != kernel_pgtab))
    panic("va must be below VIRT_KERNEL_BASE");

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return 0;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & VM_ANONYMOUS))
    return 0;

  page = pa2page(arch_vm_pte_addr(pte));

  if (--page->ref_count == 0)
    page_free_one(page);

  arch_vm_pte_clear(pte);
  arch_vm_invalidate(va);

  return 0;
}

// -----------------------------------------------------------------------------
// Architecture-specific functions
// -----------------------------------------------------------------------------

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
  return *((l2_desc_t *) pte + (L2_NR_ENTRIES * 2));
}

// Map VM flags to ARM MMU AP bits
static int flags_to_ap[] = {
  [VM_READ]                      = AP_PRIV_RO, 
  [VM_WRITE]                     = AP_PRIV_RW, 
  [VM_READ | VM_WRITE]           = AP_PRIV_RW, 
  [VM_USER | VM_READ]            = AP_USER_RO, 
  [VM_USER | VM_WRITE]           = AP_BOTH_RW, 
  [VM_USER | VM_READ | VM_WRITE] = AP_BOTH_RW, 
};

static void
arch_vm_pte_set_flags(l2_desc_t *pte, int flags)
{
  *(pte + (L2_NR_ENTRIES * 2)) = flags;
}

/**
 * Set a page table entry
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

  *(l2_desc_t *)pte = pa | bits | L2_DESC_TYPE_SM;
  arch_vm_pte_set_flags(pte, flags);
}

static void
arch_vm_pte_clear(void *pte)
{
  *(l2_desc_t *) pte = 0;
  arch_vm_pte_set_flags((l2_desc_t *) pte, 0);
}

static inline void
arch_vm_section_set(l1_desc_t *tte, physaddr_t pa, int prot)
{
  int flags;

  flags = L1_DESC_SECT_AP(flags_to_ap[prot & 7]);
  if ((prot & VM_USER) && !(prot & VM_EXEC))
    flags |= L1_DESC_SECT_XN;
  if (!(prot & VM_NOCACHE))
    flags |= (L1_DESC_SECT_B | L1_DESC_SECT_C);

  *tte = pa | flags | L1_DESC_TYPE_SECT;
}

static void
arch_vm_invalidate(uintptr_t va)
{
  cp15_tlbimva(va);
}

//
// Returns the pointer to the page table entry in the translation table pgtab
// that corresponds to the virtual address va. If alloc is not zero, allocate
// any required page tables.
//
static void *
arch_vm_lookup(void *pgtab, uintptr_t va, int alloc)
{
  l1_desc_t *trtab, *tte;
  l2_desc_t *pte;

  trtab = (l1_desc_t *) pgtab;
  tte = &trtab[L1_IDX(va)];
  if ((*tte & L1_DESC_TYPE_MASK) == L1_DESC_TYPE_FAULT) {
    struct Page *page;
    physaddr_t pa;

    if (!alloc || (page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
    
    page->ref_count++;

    pa = page2pa(page);

    // We could fit four page tables into one physical page. But since we
    // also need to store some metadata for each entry, we store only two
    // page tables in the bottom of the allocated physical page.
    trtab[(L1_IDX(va) & ~1) + 0] = pa | L1_DESC_TYPE_TABLE;
    trtab[(L1_IDX(va) & ~1) + 1] = (pa + L2_TABLE_SIZE) | L1_DESC_TYPE_TABLE;
  } else if ((*tte & L1_DESC_TYPE_MASK) != L1_DESC_TYPE_TABLE) {
    panic("not a page table");
  }

  pte = PA2KVA(L1_DESC_TABLE_BASE(*tte));
  return &pte[L2_IDX(va)];
}

// 
// Map [va, va + n) of virtual address space to physical [pa, pa + n) in
// the translation page pgtab.
//
// This function is only intended to set up static mappings in the kernel's
// portion of address space.
//
static void
arch_vm_fixed_map(l1_desc_t *pgtab, uintptr_t va, uint32_t pa, size_t n,
                  int prot)
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

      tte = &pgtab[L1_IDX(va)];
      if (*tte)
        panic("remap");

      arch_vm_section_set(tte, pa, prot);

      va += L1_SECTION_SIZE;
      pa += L1_SECTION_SIZE;
      n  -= L1_SECTION_SIZE;
    } else {
      l2_desc_t *pte;

      if ((pte = arch_vm_lookup(pgtab, va, 1)) == NULL)
        panic("out of memory");
      if (arch_vm_pte_valid(pte))
        panic("remap %p", *pte);

      arch_vm_pte_set(pte, pa, prot);

      va += PAGE_SIZE;
      pa += PAGE_SIZE;
      n  -= PAGE_SIZE;
    }
  }
}

static void *
arch_vm_create_kernel(void)
{
  extern uint8_t _start[];

  struct Page *page;
  l1_desc_t *trtab;

  // Allocate the master translation table
  if ((page = page_alloc_block(2, PAGE_ALLOC_ZERO)) == NULL)
    return NULL;

  trtab = (l1_desc_t *) page2kva(page);

  // Map all physical memory at VIRT_KERNEL_BASE
  // Permissions: kernel RW, user NONE
  arch_vm_fixed_map(trtab,
                    VIRT_KERNEL_BASE, 0,
                    PHYS_LIMIT,
                    VM_READ | VM_WRITE);

  // Map I/O devices
  // Permissions: kernel RW, user NONE, disable cache 
  arch_vm_fixed_map(trtab,
                    VIRT_KERNEL_BASE + PHYS_LIMIT, PHYS_LIMIT,
                    VIRT_VECTOR_BASE - (VIRT_KERNEL_BASE + PHYS_LIMIT),
                    VM_READ | VM_WRITE | VM_NOCACHE);

  // Map exception vectors at VIRT_VECTOR_BASE
  // Permissions: kernel R, user NONE
  arch_vm_fixed_map(trtab,
                    VIRT_VECTOR_BASE, (physaddr_t) _start,
                    PAGE_SIZE,
                    VM_READ);

  return trtab;
}

static void
arch_vm_init_percpu(void)
{
  // Switch from the minimal entry translation table to the full translation
  // table.
  cp15_ttbr0_set(KVA2PA(kernel_pgtab));
  cp15_ttbr1_set(KVA2PA(kernel_pgtab));

  // Size of the TTBR0 translation table is 8KB.
  cp15_ttbcr_set(1);

  cp15_tlbiall();
}

void *
arch_vm_create(void)
{
  struct Page *page;

  if ((page = page_alloc_block(1, PAGE_ALLOC_ZERO)) == NULL)
    return NULL;

  page->ref_count++;

  return page2kva(page);
}

static void
arch_vm_destroy(void *pgtab)
{
  struct Page *page;
  l1_desc_t *trtab;
  l2_desc_t *pte;
  unsigned i;
  unsigned j;
  
  trtab = (l1_desc_t *) pgtab;

  for (i = 0; i < L1_IDX(VIRT_KERNEL_BASE); i += 2) {
    if (!trtab[i])
      continue;

    page = pa2page(L2_DESC_SM_BASE(trtab[i]));

    pte = (l2_desc_t *) page2kva(page);
    for (j = 0; j < L2_NR_ENTRIES * 2; j++)
      assert(pte[j] == 0);

    if (--page->ref_count == 0)
      page_free_one(page);
  }

  page = kva2page(trtab);
  if (--page->ref_count == 0)
      page_free_block(page, 1);
}

static void
arch_vm_load(void *pgtab)
{
  cp15_ttbr0_set(KVA2PA(pgtab));
  cp15_tlbiall();
}
