#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <kernel/core/spinlock.h>
#include <kernel/console.h>
#include <kernel/page.h>
#include <kernel/vm.h>
#include <kernel/types.h>
#include <kernel/fs/fs.h>
#include <kernel/ipc.h>
#include <kernel/process.h>

static int vm_page_lookup(void *, uintptr_t, struct Page **, int *, int);
static int vm_page_insert(void *, struct Page *, uintptr_t, int);
static int vm_page_remove(void *, uintptr_t);

static struct KSpinLock vm_lock = K_SPINLOCK_INITIALIZER("vm");

int
vm_user_iterate(void *pgtab, uintptr_t va, size_t n,
             int (*cb)(void *, void *, size_t), void *cb_arg,
             int check_perm)
{
  struct Page *page;
  int ncopy, offset;
  int r;

  k_assert((va < VIRT_KERNEL_BASE) && ((va + n) >= va));

  while (n != 0) {
    uint8_t *kva;
    int flags;
  
    k_spinlock_acquire(&vm_lock);

    r = vm_page_lookup(pgtab, va, &page, &flags, check_perm & VM_WRITE);
    if (r < 0) {
      k_spinlock_release(&vm_lock);
      return r;
    }

    if ((flags & check_perm) != check_perm) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    page = page_inc_ref(page);

    k_spinlock_release(&vm_lock);

    kva = (uint8_t *) page2kva(page);
    offset = va % PAGE_SIZE;
    ncopy = MIN(PAGE_SIZE - offset, n);

    r = cb(cb_arg, kva + offset, ncopy);

    page_dec_ref(page, 0);

    if (r < ncopy)
      return r < 0 ? r : 0;

    va += ncopy;
    n -= ncopy;
  }

  return 0;
}

static int
vm_page_lookup(void *pgtab, uintptr_t va, struct Page **page_store, int *flags_store, int cow)
{
  struct Page *page;
  int flags;
  void *pte;

  k_assert(k_spinlock_holding(&vm_lock));

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return -EFAULT;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & VM_PAGE))
    return -EFAULT;

  flags = arch_vm_pte_flags(pte);
  page = pa2page(arch_vm_pte_addr(pte));

#ifndef NDEBUG
  page_assert(page, 0, PAGE_TAG_ANON);
#endif

  if ((flags & VM_COW) && cow) {
    int r;

    flags &= ~VM_COW;
    flags |= VM_WRITE;

    if (page->ref_count == 1) {
      // If this is the only one occurence of the page, simply re-insert it with
      // new permissions
      if ((r = vm_page_insert(pgtab, page, va, flags)) < 0)
        return r;
    } else {
      // Otherwise, insert a copy of the entire page in its place
      struct Page *page_copy;

      if ((page_copy = page_alloc_one(0, PAGE_TAG_ANON)) == NULL)
        return -ENOMEM;

      memmove(page2kva(page_copy), page2kva(page), PAGE_SIZE);

      if ((r = vm_page_insert(pgtab, page_copy, va, flags)) < 0) {
        page_free_one(page_copy);
        return r;
      }

      page = page_copy;
    }
  }

  if (flags_store)
    *flags_store = flags;
  if (page_store)
    *page_store = page;

  return 0;
}

static int
vm_page_insert(void *pgtab, struct Page *page, uintptr_t va, int flags)
{
  void *pte;

  k_assert(k_spinlock_holding(&vm_lock));

  if ((pte = arch_vm_lookup(pgtab, va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference counter before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions
  page->ref_count++;

  // If present, remove the previous mapping
  vm_page_remove(pgtab, va);

  arch_vm_pte_set(pte, page2pa(page), flags | VM_PAGE);

  return 0;
}

static int
vm_page_remove(void *pgtab, uintptr_t va)
{
  struct Page *page;
  void *pte;

  k_assert(k_spinlock_holding(&vm_lock));

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return 0;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & VM_PAGE))
    return 0;

  page = pa2page(arch_vm_pte_addr(pte));

#ifndef NDEBUG
  page_assert(page, 0, PAGE_TAG_ANON);
#endif

  if (--page->ref_count == 0)
    page_free_one(page);

  arch_vm_pte_clear(pte);
  arch_vm_invalidate(va);

  return 0;
}

static void
vm_user_assert_pages(uintptr_t start_va, uintptr_t end_va)
{
  k_assert((start_va % PAGE_SIZE) == 0);
  k_assert((start_va < VIRT_KERNEL_BASE) && (end_va >= start_va));
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
  uintptr_t end_va = ROUND_UP(start_va + n, PAGE_SIZE);

  vm_user_assert_pages(start_va, end_va);

  for (uintptr_t va = start_va; va < end_va; va += PAGE_SIZE) {
    k_spinlock_acquire(&vm_lock);
    vm_page_remove(vm, va);
    k_spinlock_release(&vm_lock);
  }
}

int
vm_user_clone(void *src, void *dst, uintptr_t start_va, size_t n, int share)
{
  uintptr_t end_va = ROUND_UP(start_va + n, PAGE_SIZE);

  vm_user_assert_pages(start_va, end_va);
 
  for (uintptr_t va = start_va ; va < end_va; va += PAGE_SIZE) {
    struct Page *page;
    int flags, r;

    k_spinlock_acquire(&vm_lock);

    if ((r = vm_page_lookup(src, va, &page, &flags, share)) < 0) {
      k_spinlock_release(&vm_lock);
      return r;
    }

    if (!share && (flags & VM_WRITE)) {
      flags &= ~VM_WRITE;
      flags |= VM_COW;

      if ((r = vm_page_insert(src, page, va, flags)) < 0) {
        k_spinlock_release(&vm_lock);
        return r;
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

int
vm_handle_fault(void *pgtab, uintptr_t va)
{
  struct Page *page;
  int flags;
  int r;

  if ((va < PAGE_SIZE) || (va >= VIRT_KERNEL_BASE))
    return -EFAULT;

  k_spinlock_acquire(&vm_lock);

  if ((r = vm_page_lookup(pgtab, va, &page, &flags, 1)) < 0) {
    k_spinlock_release(&vm_lock);
    return r;
  }

  if (!(flags & VM_WRITE))
    r = -EFAULT;

  k_spinlock_release(&vm_lock);
  
  return r;
}
