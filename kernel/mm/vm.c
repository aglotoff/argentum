#include <errno.h>
#include <kernel/cprintf.h>
#include <kernel/page.h>
#include <kernel/vm.h>
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <string.h>
#include <sys/mman.h>
#include <kernel/fs/fs.h>

static struct KSpinLock vm_lock = K_SPINLOCK_INITIALIZER("vm_lock");

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

  assert(k_spinlock_holding(&vm_lock));

  if ((pte = vm_arch_lookup(pgtab, va, 0)) == NULL)
    return NULL;

  if (!vm_arch_pte_valid(pte) || !(vm_arch_pte_flags(pte) & VM_PAGE))
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

  assert(k_spinlock_holding(&vm_lock));

  if ((pte = vm_arch_lookup(pgtab, va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference counter before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions
  page->ref_count++;

  // If present, remove the previous mapping
  vm_page_remove(pgtab, (uintptr_t) va);

  vm_arch_pte_set(pte, page2pa(page), flags | VM_PAGE);

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

  assert(k_spinlock_holding(&vm_lock));

  if ((pte = vm_arch_lookup(pgtab, va, 0)) == NULL)
    return 0;

  if (!vm_arch_pte_valid(pte) || !(vm_arch_pte_flags(pte) & VM_PAGE))
    return 0;

  page = pa2page(vm_arch_pte_addr(pte));

  if (--page->ref_count == 0)
    page_free_one(page);

  vm_arch_pte_clear(pte);
  vm_arch_invalidate(va);

  return 0;
}

static struct Page *
vm_page_cow(void *pgtab, uintptr_t va, struct Page *page, int flags)
{
  struct Page *page_copy;

  assert(flags & VM_COW);

  flags &= ~VM_COW;
  flags |= VM_WRITE;

  // If this is the only one occurence of the page, simply re-insert it with
  // new permissions
  if (page->ref_count == 1) {
    if (vm_page_insert(pgtab, page, va, flags) < 0)
      return NULL;
    return page;
  }

  // Otherwise, insert a copy of the entire page in its place
  if ((page_copy = page_alloc_one(PAGE_ALLOC_ZERO, PAGE_TAG_ANON)) == NULL)
    return NULL;

  memmove(page2kva(page_copy), page2kva(page), PAGE_SIZE);

  if (vm_page_insert(pgtab, page_copy, va, flags) < 0) {
    page_free_one(page_copy);
    return NULL;
  }

  return page_copy;
}

int
vm_page_lookup_cow(void *pgtab, uintptr_t va, struct Page **page_store,
                   int *flags_store)
{
  struct Page *page;
  int flags;

  if ((page = vm_page_lookup(pgtab, va, &flags)) == NULL)
    return -EFAULT;
  
  if (flags & VM_COW) {
    if ((page = vm_page_cow(pgtab, va, page, flags)) == NULL)
      return -ENOMEM;
  }
  
  if (page_store != NULL)
    *page_store = page;
  if (flags_store != NULL)
    *flags_store = flags;

  return 0;
}

static void
vm_user_assert(uintptr_t start_va, uintptr_t end_va)
{
  if ((start_va >= VIRT_KERNEL_BASE) || (end_va < start_va))
    panic("invalid va range: [%p,%p)", start_va, end_va);
}

static void
vm_user_assert_pages(uintptr_t start_va, uintptr_t end_va)
{
  if ((start_va % PAGE_SIZE) != 0)
    panic("va not page-aligned %p", start_va);
  if ((start_va >= VIRT_KERNEL_BASE) || (end_va < start_va))
    panic("invalid va range: [%p,%p)", start_va, end_va);
}

int
vm_copy_out(void *pgtab, const void *src, uintptr_t dst_va, size_t n)
{
  uint8_t *p = (uint8_t *) src;

  vm_user_assert(dst_va, dst_va + n);

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;
    int r;

    offset = dst_va % PAGE_SIZE;
    ncopy = MIN(PAGE_SIZE - offset, n);

    k_spinlock_acquire(&vm_lock);

    if ((r = vm_page_lookup_cow(pgtab, dst_va, &page, NULL)) < 0) {
      k_spinlock_release(&vm_lock);
      return r;
    }

    kva = (uint8_t *) page2kva(page);
    memmove(kva + offset, p, ncopy);

    k_spinlock_release(&vm_lock);

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

  vm_user_assert(src_va, src_va + n);

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    offset = src_va % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    k_spinlock_acquire(&vm_lock);

    if ((page = vm_page_lookup(vm, src_va, NULL)) == NULL) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    kva = (uint8_t *) page2kva(page);
    memmove(p, kva + offset, ncopy);

    k_spinlock_release(&vm_lock);

    src_va += ncopy;
    p      += ncopy;
    n      -= ncopy;
  }

  return 0;
}

int
vm_user_alloc(void *vm, uintptr_t start_va, size_t n, int flags)
{
  struct Page *page;
  uintptr_t va, end_va;
  int r;

  end_va = ROUND_UP(start_va + n, PAGE_SIZE);
  vm_user_assert_pages(start_va, end_va);

  for (va = start_va; va < end_va; va += PAGE_SIZE) {
    k_spinlock_acquire(&vm_lock);

    if ((page = page_alloc_one(PAGE_ALLOC_ZERO, PAGE_TAG_ANON)) == NULL) {
      k_spinlock_release(&vm_lock);

      vm_user_free(vm, start_va, va - start_va);
      
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm, page, va, flags)) != 0)) {
      page_free_one(page);
      k_spinlock_release(&vm_lock);

      vm_user_free(vm, start_va, va - start_va);

      return r;
    }

    k_spinlock_release(&vm_lock);
  }

  return 0;
}

void
vm_user_free(void *vm, uintptr_t start_va, size_t n)
{
  uintptr_t va, end_va;

  end_va = ROUND_UP(start_va + n, PAGE_SIZE);
  vm_user_assert_pages(start_va, end_va);

  for (va = start_va; va < end_va; va += PAGE_SIZE) {
    k_spinlock_acquire(&vm_lock);
    vm_page_remove(vm, va);
    k_spinlock_release(&vm_lock);
  }
}

int
vm_user_clone(void *src, void *dst, uintptr_t start_va, size_t n, int share)
{
  uintptr_t va, end_va;

  end_va = ROUND_UP(start_va + n, PAGE_SIZE);
  vm_user_assert_pages(start_va, end_va);
 
  for (va = start_va ; va < end_va; va += PAGE_SIZE) {
    struct Page *page;
    int flags, r;

    k_spinlock_acquire(&vm_lock);

    if (share) {
      // When creating a shared region, remove the copy-on-write bit
      if ((r = vm_page_lookup_cow(src, va, &page, &flags)) < 0) {
        k_spinlock_release(&vm_lock);
        return r;
      }
    } else {
      if ((page = vm_page_lookup(src, va, &flags)) == NULL) {
        k_spinlock_release(&vm_lock);
        return -EFAULT;
      }

      if (flags & VM_WRITE) {
        flags &= ~VM_WRITE;
        flags |= VM_COW;

        if ((r = vm_page_insert(src, page, va, flags)) < 0) {
          k_spinlock_release(&vm_lock);
          return r;
        }
      }
    }

    if ((r = vm_page_insert(dst, page, va, flags)) < 0) {
      k_spinlock_release(&vm_lock);
      return r;
    }

    k_spinlock_release(&vm_lock);
  }

  return 0;
}

static int
vm_flags_check(int curr_flags, int flags)
{
  if (curr_flags & VM_COW) {
    curr_flags &= ~VM_COW;
    curr_flags |= VM_WRITE;
  }

  return (curr_flags & flags) == flags;
}

int
vm_user_check_ptr(void *pgtab, uintptr_t va, int flags)
{
  int curr_flags;

  if (va >= VIRT_KERNEL_BASE)
    return -EFAULT;

  k_spinlock_acquire(&vm_lock);

  if (vm_page_lookup(pgtab, va, &curr_flags) == NULL) {
    k_spinlock_release(&vm_lock);
    return -EFAULT;
  }

  k_spinlock_release(&vm_lock);

  if (!vm_flags_check(curr_flags, flags))
    return -EFAULT;

  return 0;
}

int
vm_user_check_str(void *vm, uintptr_t va, size_t *len_ptr, int flags)
{
  size_t len = 0;

  while (va < VIRT_KERNEL_BASE) {
    const char *p;
    struct Page *page;
    unsigned off;
    int curr_flags;

    k_spinlock_acquire(&vm_lock);

    page = vm_page_lookup(vm, va, &curr_flags);

    if ((page == NULL) || !vm_flags_check(curr_flags, flags)) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    p = (const char *) page2kva(page);

    for (off = va % PAGE_SIZE; off < PAGE_SIZE; off++) {
      if (p[off] == '\0') {
        if (len_ptr)
          *len_ptr = len;

        k_spinlock_release(&vm_lock);

        return 0;
      }

      len++;
      va++;
    }

    k_spinlock_release(&vm_lock);
  }

  return -EFAULT;
}

int
vm_user_check_args(void *vm, uintptr_t va, size_t *len_ptr, int flags)
{
  size_t len = 0;

  if (va % sizeof(char *) != 0)
    return -EFAULT;

  while (va < VIRT_KERNEL_BASE) {
    const char **p;
    struct Page *page;
    unsigned off;
    int curr_flags;

    k_spinlock_acquire(&vm_lock);

    page = vm_page_lookup(vm, va, &curr_flags);

    if ((page == NULL) || !vm_flags_check(curr_flags, flags)) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    p = (const char **) page2kva(page);

    for (off = (va % PAGE_SIZE) / sizeof *p; off < PAGE_SIZE / sizeof *p; off++) {
      if (p[off] == NULL) {
        if (len_ptr)
          *len_ptr = len;

        k_spinlock_release(&vm_lock);

        return 0;
      }

      len++;
      va += sizeof *p;
    }

    k_spinlock_release(&vm_lock);
  }

  return -EFAULT;
}

int
vm_user_check_buf(void *pgtab, uintptr_t start_va, size_t n, int flags)
{
  struct Page *page;
  uintptr_t va, end_va;

  end_va = ROUND_UP(start_va + n, PAGE_SIZE);
  if ((start_va >= VIRT_KERNEL_BASE) || (end_va < start_va))
    return -EFAULT;

  for (va = start_va; va < end_va; va += PAGE_SIZE) {
    int r, curr_flags;

    k_spinlock_acquire(&vm_lock);

    // TODO: do not do copy-on-write before actual memory access!
    if ((r = vm_page_lookup_cow(pgtab, va, &page, &curr_flags)) < 0) {
      k_spinlock_release(&vm_lock);
      return r;
    }

    if (!vm_flags_check(curr_flags, flags)) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    k_spinlock_release(&vm_lock);
  }

  return 0;
}

int
vm_handle_fault(void *pgtab, uintptr_t va)
{
  struct Page *fault_page;
  int flags;

  if ((va < PAGE_SIZE) || (va >= VIRT_KERNEL_BASE))
    return -EFAULT;

  k_spinlock_acquire(&vm_lock);

  fault_page = vm_page_lookup(pgtab, va, &flags);

  if ((fault_page == NULL) || !(flags & VM_COW)) {
    k_spinlock_release(&vm_lock);
    return -EFAULT;
  }

  if (vm_page_cow(pgtab, va, fault_page, flags) == NULL) {
    k_spinlock_release(&vm_lock);
    return -ENOMEM;
  }

  k_spinlock_release(&vm_lock);
  
  return 0;
}

int
vm_space_load_inode(void *pgtab, void *va, struct Inode *ip, size_t n, off_t off)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    k_spinlock_acquire(&vm_lock);

    page = vm_page_lookup(pgtab, (uintptr_t) dst, NULL);
    if (page == NULL) {
      return -EFAULT;
    }

    // TODO: unsafe?

    k_spinlock_release(&vm_lock);

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    if ((r = fs_inode_read_locked(ip, kva + offset, ncopy, &off)) != ncopy)
      return r;

    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}
