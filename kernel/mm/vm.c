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

static struct Page *vm_page_lookup(void *, uintptr_t, int *);
static int vm_page_lookup_copy_on_write(void *, uintptr_t, struct Page **, int *);
static struct Page *vm_page_copy_on_write(void *, uintptr_t, struct Page *, int);
static int vm_page_insert(void *, struct Page *, uintptr_t, int);
static int vm_page_remove(void *, uintptr_t);

struct KSpinLock vm_lock = K_SPINLOCK_INITIALIZER("vm_lock");

int
vm_read(void *pgtab, uintptr_t va, size_t n, int (*cb)(void *, const void *, size_t), void *arg)
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
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    k_spinlock_release(&vm_lock);

    page = page_inc_ref(page);

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    r = cb(arg, kva + offset, ncopy);

    page_dec_ref(page, 0);

    if (r != ncopy)
      return r;

    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

static struct Page *
vm_page_lookup(void *pgtab, uintptr_t va, int *flags_store)
{
  struct Page *page;
  void *pte;

  k_assert(k_spinlock_holding(&vm_lock));

  if ((pte = arch_vm_lookup(pgtab, va, 0)) == NULL)
    return NULL;

  if (!arch_vm_pte_valid(pte) || !(arch_vm_pte_flags(pte) & VM_PAGE))
    return NULL;

  if (flags_store)
    *flags_store = arch_vm_pte_flags(pte);

  page = pa2page(arch_vm_pte_addr(pte));
  page_assert(page, 0, PAGE_TAG_ANON);
  return page;
}

int
vm_write(void *pgtab, uintptr_t va, size_t n, int (*cb)(void *, void *, size_t), void *arg)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    k_spinlock_acquire(&vm_lock);

    r = vm_page_lookup_copy_on_write(pgtab, (uintptr_t) dst, &page, NULL);
    if (r != 0) {
      k_spinlock_release(&vm_lock);
      return -EFAULT;
    }

    k_spinlock_release(&vm_lock);

    page = page_inc_ref(page);

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    r = cb(arg, kva + offset, ncopy);

    page_dec_ref(page, 0);

    if (r != ncopy)
      return r;

    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

static int
vm_page_lookup_copy_on_write(void *pgtab, uintptr_t va, struct Page **page_store,
                   int *flags_store)
{
  struct Page *page;
  int flags;

  if ((page = vm_page_lookup(pgtab, va, &flags)) == NULL)
    return -EFAULT;
  
  if (flags & VM_COW) {
    if ((page = vm_page_copy_on_write(pgtab, va, page, flags)) == NULL)
      return -ENOMEM;
  }
  
  if (page_store != NULL)
    *page_store = page;
  if (flags_store != NULL)
    *flags_store = flags;

  return 0;
}

static struct Page *
vm_page_copy_on_write(void *pgtab, uintptr_t va, struct Page *page, int flags)
{
  struct Page *page_copy;

  k_assert(flags & VM_COW);

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
  page_assert(page, 0, PAGE_TAG_ANON);

  if (--page->ref_count == 0)
    page_free_one(page);

  arch_vm_pte_clear(pte);
  arch_vm_invalidate(va);

  return 0;
}

static void
vm_user_assert(uintptr_t start_va, uintptr_t end_va)
{
  if ((start_va >= VIRT_KERNEL_BASE) || (end_va < start_va))
    k_panic("invalid va range: [%p,%p)", start_va, end_va);
}

static void
vm_user_assert_pages(uintptr_t start_va, uintptr_t end_va)
{
  if ((start_va % PAGE_SIZE) != 0)
    k_panic("va not page-aligned %p", start_va);
  if ((start_va >= VIRT_KERNEL_BASE) || (end_va < start_va))
    k_panic("invalid va range: [%p,%p)", start_va, end_va);
}

static int
vm_user_copy_out_callback(void *arg, void *kva, size_t n)
{
  uint8_t **src = (uint8_t **) arg;
  memmove(kva, *src, n);
  *src += n;
  return n;
}

int
vm_user_copy_out(void *pgtab, const void *src, uintptr_t dst_va, size_t n)
{
  uint8_t *p = (uint8_t *) src;

  vm_user_assert(dst_va, dst_va + n);
  return vm_write(pgtab, dst_va, n, vm_user_copy_out_callback, &p);
}

static int
vm_user_copy_in_callback(void *arg, const void *kva, size_t n)
{
  uint8_t **dst = (uint8_t **) arg;
  memmove(*dst, kva, n);
  *dst += n;
  return n;
}

int
vm_user_copy_in(void *pgtab, void *dst, uintptr_t src_va, size_t n)
{
  uint8_t *p = (uint8_t *) dst;

  vm_user_assert(src_va, src_va + n);
  return vm_read(pgtab, src_va, n, vm_user_copy_in_callback, &p);
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
      if ((r = vm_page_lookup_copy_on_write(src, va, &page, &flags)) < 0) {
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
    if ((r = vm_page_lookup_copy_on_write(pgtab, va, &page, &curr_flags)) < 0) {
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
  struct Page *page;
  int flags;

  if ((va < PAGE_SIZE) || (va >= VIRT_KERNEL_BASE))
    return -EFAULT;

  k_spinlock_acquire(&vm_lock);

  page = vm_page_lookup(pgtab, va, &flags);

  if ((page == NULL) || !(flags & VM_COW)) {
    k_spinlock_release(&vm_lock);
    return -EFAULT;
  }

  if (vm_page_copy_on_write(pgtab, va, page, flags) == NULL) {
    k_spinlock_release(&vm_lock);
    return -ENOMEM;
  }

  k_spinlock_release(&vm_lock);
  
  return 0;
}
