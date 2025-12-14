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
vm_user_read(void *pgtab, uintptr_t va, size_t n, int (*cb)(void *, const void *, size_t), void *arg, int check)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    int flags;
  
    k_spinlock_acquire(&vm_lock);

    page = vm_page_lookup(pgtab, (uintptr_t) dst, &flags);
    if ((page == NULL) || (check && !(flags & VM_READ))) {
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

    if (r < ncopy)
      return r < 0 ? r : 0;

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
vm_user_write(void *pgtab, uintptr_t va, size_t n, int (*cb)(void *, void *, size_t), void *arg, int check)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    int flags;

    k_spinlock_acquire(&vm_lock);

    r = vm_page_lookup_copy_on_write(pgtab, (uintptr_t) dst, &page, &flags);
    if ((r != 0) || (check && !(flags & VM_WRITE))) {
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

    if (r < ncopy)
      return r < 0 ? r : 0;

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
    
    flags &= ~VM_COW;
    flags |= VM_WRITE;
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
  return vm_user_write(pgtab, dst_va, n, vm_user_copy_out_callback, &p, 1);
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
  return vm_user_read(pgtab, src_va, n, vm_user_copy_in_callback, &p, 1);
}

struct copy_in_string {
  char *buf;
  size_t i;
  size_t max_len;
};

static int
vm_user_copy_in_string_callback(void *arg, const void *kva, size_t n)
{
  struct copy_in_string *ctx = (struct copy_in_string *) arg;
  const char *src = (const char *) kva;
  size_t i;

  for (i = 0; i < n; i++) {
    if ((ctx->buf[ctx->i++] = src[i]) == '\0')
      return 0;
    if (ctx->i >= ctx->max_len)
      return -ENAMETOOLONG;
  }

  return n;
}

int
vm_user_copy_in_string(void *pgtab, char *dst, uintptr_t src_va, size_t max_len)
{
  struct copy_in_string ctx = { dst, 0, max_len };
  int r;

  k_assert(max_len > 0);

  vm_user_assert(src_va, src_va + max_len);

  r = vm_user_read(pgtab, src_va, max_len, vm_user_copy_in_string_callback, &ctx, 1);
  if (r < 0)
    return r;

  return ctx.i;
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
