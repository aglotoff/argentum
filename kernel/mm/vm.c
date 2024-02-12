#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <string.h>
#include <sys/mman.h>

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

static struct SpinLock vm_lock = SPIN_INITIALIZER("vm");

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

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & _PROT_PAGE))
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

  arch_vm_pte_set(pte, page2pa(page), flags | _PROT_PAGE);

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

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & _PROT_PAGE))
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
static int prot_to_ap[] = {
  [PROT_READ]                           = AP_PRIV_RO, 
  [PROT_WRITE]                          = AP_PRIV_RW, 
  [PROT_READ  | PROT_WRITE]             = AP_PRIV_RW, 
  [_PROT_USER | PROT_READ ]             = AP_USER_RO, 
  [_PROT_USER | PROT_WRITE]             = AP_BOTH_RW, 
  [_PROT_USER | PROT_READ | PROT_WRITE] = AP_BOTH_RW, 
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

  bits = L2_DESC_AP(prot_to_ap[flags & (PROT_WRITE | PROT_READ | _PROT_USER)]);
  if ((flags & _PROT_USER) && !(flags & PROT_EXEC))
    bits |= L2_DESC_SM_XN;
  if (!(flags & PROT_NOCACHE))
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

  bits = L1_DESC_SECT_AP(prot_to_ap[flags & (PROT_WRITE | PROT_READ | _PROT_USER)]);
  if ((flags & _PROT_USER) && !(flags & PROT_EXEC))
    bits |= L1_DESC_SECT_XN;
  if (!(flags & PROT_NOCACHE))
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
  arch_vm_fixed_map(VIRT_KERNEL_BASE, 0, PHYS_LIMIT, PROT_READ | PROT_WRITE);

  // Map I/O devices
  // Permissions: kernel RW, user NONE, disable cache 
  arch_vm_fixed_map(VIRT_KERNEL_BASE + PHYS_LIMIT, PHYS_LIMIT,
                    VIRT_VECTOR_BASE - (VIRT_KERNEL_BASE + PHYS_LIMIT),
                    PROT_READ | PROT_WRITE | PROT_NOCACHE);

  // Map exception vectors at VIRT_VECTOR_BASE
  // Permissions: kernel R, user NONE
  arch_vm_fixed_map(VIRT_VECTOR_BASE, (physaddr_t) _start, PAGE_SIZE, PROT_READ);
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

int
vm_range_alloc(void *vm, uintptr_t va, size_t n, int prot)
{
  struct Page *page;
  uint8_t *a, *start, *end;
  int r;

  start = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end   = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((start > end) || (end > (uint8_t *) VIRT_KERNEL_BASE))
    return -EINVAL;

  for (a = start; a < end; a += PAGE_SIZE) {
    spin_lock(&vm_lock);

    if ((page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
      vm_range_free(vm, (uintptr_t) start, a - start);
      spin_unlock(&vm_lock);
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm, page, (uintptr_t) a, prot)) != 0)) {
      page_free_one(page);
      spin_unlock(&vm_lock);

      vm_range_free(vm, (uintptr_t) start, a - start);
      return r;
    }

    spin_unlock(&vm_lock);
  }

  return 0;
}

void
vm_range_free(void *vm, uintptr_t va, size_t n)
{
  uint8_t *a, *end;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((a > end) || (end > (uint8_t *) VIRT_KERNEL_BASE))
    panic("invalid range [%p,%p)", a, end);

  for ( ; a < end; a += PAGE_SIZE) {
    spin_lock(&vm_lock);
    vm_page_remove(vm, (uintptr_t) a);
    spin_unlock(&vm_lock);
  }
}

int
vm_range_clone(void *src, void *dst, uintptr_t va, size_t n, int share)
{
  struct Page *src_page, *dst_page;
  uint8_t *a, *end;
  int r;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  for ( ; a < end; a += PAGE_SIZE) {
    int perm;

    spin_lock(&vm_lock);

    if ((src_page = vm_page_lookup(src, (uintptr_t) a, &perm)) == NULL) {
      spin_unlock(&vm_lock);
      continue;
    }
    
    if (share) {
      if (perm & _PROT_COW) {
        if ((dst_page = page_alloc_one(0)) == NULL) {
          spin_unlock(&vm_lock);
          return -ENOMEM;
        }

        perm |= PROT_WRITE;
        perm &= ~_PROT_COW;

        memmove(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);

        if ((r = vm_page_insert(src, dst_page, (uintptr_t) va, perm)) < 0) {
          page_free_one(dst_page);
          spin_unlock(&vm_lock);
          return r;
        }

        if ((r = vm_page_insert(dst, dst_page, (uintptr_t) va, perm)) < 0) {
          spin_unlock(&vm_lock);
          return r;
        }
      } else {
        if ((r = vm_page_insert(dst, src_page, (uintptr_t) a, perm)) < 0) {
          spin_unlock(&vm_lock);
          return r;
        }
      }
    } else if ((perm & PROT_WRITE) || (perm & _PROT_COW)) {
      perm &= ~PROT_WRITE;
      perm |= _PROT_COW;

      if ((r = vm_page_insert(src, src_page, (uintptr_t) a, perm)) < 0) {
        spin_unlock(&vm_lock);
        return r;
      }

      if ((r = vm_page_insert(dst, src_page, (uintptr_t) a, perm)) < 0) {
        spin_unlock(&vm_lock);
        return r;
      }
    } else {
      if ((dst_page = page_alloc_one(0)) == NULL) {
        spin_unlock(&vm_lock);
        return -ENOMEM;
      }

      if ((r = vm_page_insert(dst, dst_page, (uintptr_t) va, perm)) < 0) {
        page_free_one(dst_page);
        spin_unlock(&vm_lock);
        return r;
      }

      memcpy(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
    }

    spin_unlock(&vm_lock);
  }

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * Copying Data Between Address Spaces
 * ----------------------------------------------------------------------------
 */

int
vm_copy_out(void *pgtab, const void *src, uintptr_t dst_va, size_t n)
{
  uint8_t *p = (uint8_t *) src;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;
    int prot;

    offset = dst_va % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    spin_lock(&vm_lock);

    if ((page = vm_page_lookup(pgtab, dst_va, &prot)) == NULL) {
      spin_unlock(&vm_lock);
      return -EFAULT;
    }

    // Do copy-on-write
    if (prot & _PROT_COW) {
      int r;

      prot &= ~_PROT_COW;
      prot |= PROT_WRITE;

      if (page->ref_count == 1) {
        // The page has only one reference, just update permission bits
        if ((r = vm_page_insert(pgtab, page, dst_va, prot)) < 0) {
          spin_unlock(&vm_lock);
          return r;
        }
      } else {
        // Copy the page contents and insert with new permissions
        struct Page *page_copy;

        if ((page_copy = page_alloc_one(0)) == NULL) {
          spin_unlock(&vm_lock);
          return -ENOMEM;
        }
        
        memmove(page2kva(page_copy), page2kva(page), PAGE_SIZE);

        if ((r = vm_page_insert(pgtab, page_copy, dst_va, prot)) < 0) {
          page_free_one(page_copy);
          spin_unlock(&vm_lock);
          return r;
        }
      }
    }
    
    kva = (uint8_t *) page2kva(page);
    memmove(kva + offset, p, ncopy);

    spin_unlock(&vm_lock);

    p      += ncopy;
    dst_va += ncopy;
    n      -= ncopy;
  }

  return 0;
}

int
vm_copy_in(void *vm, void *dst, uintptr_t src_va, size_t n)
{
  uint8_t *p = (uint8_t *) dst;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    offset = src_va % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    spin_lock(&vm_lock);

    if ((page = vm_page_lookup(vm, src_va, NULL)) == NULL) {
      spin_unlock(&vm_lock);
      return -EFAULT;
    }

    kva = (uint8_t *) page2kva(page);
    memmove(p, kva + offset, ncopy);

    spin_unlock(&vm_lock);

    src_va += ncopy;
    p      += ncopy;
    n      -= ncopy;
  }

  return 0;
}
