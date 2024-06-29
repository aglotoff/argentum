#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/page.h>
#include <kernel/vm.h>
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <string.h>
#include <sys/mman.h>

static struct KSpinLock vm_spin = K_SPINLOCK_INITIALIZER("vm_spin");

static inline void
vm_lock(void)
{
  k_spinlock_acquire(&vm_spin);
}

static inline void
vm_unlock(void)
{
  k_spinlock_release(&vm_spin);
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

  if ((pte = vm_arch_lookup(pgtab, va, 0)) == NULL)
    return NULL;

  if (!vm_arch_pte_valid(pte) || !(vm_arch_pte_flags(pte) & _PROT_PAGE))
    return NULL;

  if (flags_store)
    *flags_store = vm_arch_pte_flags(pte);

  return pa2page(vm_arch_pte_addr(pte));
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

  if ((pte = vm_arch_lookup(pgtab, va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions
  page->ref_count++;

  // If present, remove the previous mapping
  vm_page_remove(pgtab, (uintptr_t) va);

  vm_arch_pte_set(pte, page2pa(page), flags | _PROT_PAGE);

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

  if ((pte = vm_arch_lookup(pgtab, va, 0)) == NULL)
    return 0;

  if (!vm_arch_pte_valid(pte) || !(vm_arch_pte_flags(pte) & _PROT_PAGE))
    return 0;

  page = pa2page(vm_arch_pte_addr(pte));

  if (--page->ref_count == 0)
    page_free_one(page);

  vm_arch_pte_clear(pte);
  vm_arch_invalidate(va);

  return 0;
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
    vm_lock();

    if ((page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
      vm_unlock();

      vm_range_free(vm, (uintptr_t) start, a - start);
      
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm, page, (uintptr_t) a, prot)) != 0)) {
      page_free_one(page);
      vm_unlock();

      vm_range_free(vm, (uintptr_t) start, a - start);

      return r;
    }

    vm_unlock();
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
    vm_lock();
    vm_page_remove(vm, (uintptr_t) a);
    vm_unlock();
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

    vm_lock();

    if ((src_page = vm_page_lookup(src, (uintptr_t) a, &perm)) == NULL) {
      vm_unlock();
      continue;
    }

    if (share) {
      if (perm & _PROT_COW) {
        perm |= PROT_WRITE;
        perm &= ~_PROT_COW;

        if (src_page->ref_count == 1) {
          if ((r = vm_page_insert(src, src_page, va, perm)) < 0) {
            vm_unlock();

            page_free_one(dst_page);

            return r;
          }
        } else {
          if ((dst_page = page_alloc_one(0)) == NULL) {
            vm_unlock();
            return -ENOMEM;
          }

          if ((r = vm_page_insert(src, dst_page, va, perm)) < 0) {
            page_free_one(dst_page);
            vm_unlock();
            return r;
          }

          memmove(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
          src_page = dst_page;
        }
      }

      if ((r = vm_page_insert(dst, src_page, va, perm)) < 0) {
        vm_unlock();
        return r;
      }
    } else if ((perm & PROT_WRITE) || (perm & _PROT_COW)) {
      perm &= ~PROT_WRITE;
      perm |= _PROT_COW;

      if ((r = vm_page_insert(src, src_page, (uintptr_t) a, perm)) < 0) {
        vm_unlock();
        return r;
      }

      if ((r = vm_page_insert(dst, src_page, (uintptr_t) a, perm)) < 0) {
        vm_unlock();
        return r;
      }
    } else {
      if ((dst_page = page_alloc_one(0)) == NULL) {
        vm_unlock();
        return -ENOMEM;
      }

      if ((r = vm_page_insert(dst, dst_page, (uintptr_t) va, perm)) < 0) {
        vm_unlock();

        page_free_one(dst_page);
        return r;
      }

      memcpy(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
    }

    vm_unlock();
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

    vm_lock();

    if ((page = vm_page_lookup(pgtab, dst_va, &prot)) == NULL) {
      vm_unlock();
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
          vm_unlock();
          return r;
        }
      } else {
        // Copy the page contents and insert with new permissions
        struct Page *page_copy;

        if ((page_copy = page_alloc_one(0)) == NULL) {
          vm_unlock();
          return -ENOMEM;
        }
        
        memmove(page2kva(page_copy), page2kva(page), PAGE_SIZE);

        if ((r = vm_page_insert(pgtab, page_copy, dst_va, prot)) < 0) {
          vm_unlock();

          page_free_one(page_copy);
          return r;
        }
      }
    }
    
    kva = (uint8_t *) page2kva(page);
    memmove(kva + offset, p, ncopy);

    vm_unlock();

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

    vm_lock();

    if ((page = vm_page_lookup(vm, src_va, NULL)) == NULL) {
      vm_unlock();
      return -EFAULT;
    }

    kva = (uint8_t *) page2kva(page);
    memmove(p, kva + offset, ncopy);

    vm_unlock();

    src_va += ncopy;
    p      += ncopy;
    n      -= ncopy;
  }

  return 0;
}

int
vm_check_str(void *vm, uintptr_t va, size_t *len_ptr, int perm)
{
  size_t len = 0;

  while (va < VIRT_KERNEL_BASE) {
    const char *p;
    struct Page *page;
    unsigned off;
    int flags;

    vm_lock();

    page = vm_page_lookup(vm, va, &flags);

    if ((page == NULL) || ((flags & perm) != perm)) {
      vm_unlock();
      return -EFAULT;
    }

    p = (const char *) page2kva(page);

    for (off = va % PAGE_SIZE; off < PAGE_SIZE; off++) {
      if (p[off] == '\0') {
        if (len_ptr)
          *len_ptr = len;

        vm_unlock();

        return 0;
      }

      len++;
    }

    vm_unlock();

    va += PAGE_SIZE - (va % PAGE_SIZE);
  }

  return -EFAULT;
}
