#include <kernel.h>
#include <page.h>
#include <vm.h>

// Master kernel page table.
void *kernel_pgtab;

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
