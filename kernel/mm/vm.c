#include <assert.h>
#include <errno.h>
#include <string.h>

#include <cprintf.h>
#include <drivers/console.h>
#include <fs/fs.h>
#include <types.h>
#include <mm/kobject.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <process.h>

#include <mm/vm.h>

static struct KObjectPool *vm_pool;
// static struct KObjectPool *vm_area_pool;

static struct Page *vm_page_lookup(l1_desc_t *, const void *, l2_desc_t **);
static int          vm_page_insert(l1_desc_t *, struct Page *, void *, unsigned);
static void         vm_page_remove(l1_desc_t *, void *);

/*
 * ----------------------------------------------------------------------------
 * Page Frame Mapping Operations
 * ----------------------------------------------------------------------------
 */

static struct Page *
vm_page_lookup(l1_desc_t *trtab, const void *va, l2_desc_t **pte_store)
{
  l2_desc_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);

  pte = mmu_pte_get(trtab, (uintptr_t ) va, 0);

  if (pte_store)
    *pte_store = pte;

  if ((pte == NULL) || !mmu_pte_valid(pte))
    return NULL;

  return pa2page(mmu_pte_base(pte));
}

static int
vm_page_insert(l1_desc_t *trtab, struct Page *page, void *va, unsigned perm)
{
  l2_desc_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);



  if ((pte = mmu_pte_get(trtab, (uintptr_t ) va, 1)) == NULL)
    return -ENOMEM;

  // Incrementing the reference count before calling vm_page_remove() allows
  // us to elegantly handle the situation when the same page is re-inserted at
  // the same virtual address, but with different permissions.
  page->ref_count++;

  // If present, remove the previous mapping.
  vm_page_remove(trtab, va);

  mmu_pte_set(pte, page2pa(page), perm);

  return 0;
}

static void
vm_page_remove(l1_desc_t *trtab, void *va)
{
  struct Page *page;
  l2_desc_t *pte;

  if ((uintptr_t) va >= KERNEL_BASE)
    panic("bad va: %p", va);

  if ((page = vm_page_lookup(trtab, va, &pte)) == NULL)
    return;

  if (--page->ref_count == 0)
    page_free_one(page);

  mmu_pte_clear(pte);
  mmu_invalidate_va(va);
}

/*
 * ----------------------------------------------------------------------------
 * Managing Regions of Memory
 * ----------------------------------------------------------------------------
 */

int
vm_user_alloc(struct VM *vm, void *va, size_t n, int prot)
{
  struct Page *page;
  uint8_t *a, *start, *end;
  int r;

  start = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end   = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((start > end) || (end > (uint8_t *) KERNEL_BASE))
    panic("invalid range [%p,%p)", start, end);

  for (a = start; a < end; a += PAGE_SIZE) {
    if ((page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
      vm_user_dealloc(vm, start, a - start);
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm->trtab, page, a, prot)) != 0)) {
      page_free_one(page);
      vm_user_dealloc(vm, start, a - start);
      return r;
    }
  }

  return 0;
}

void
vm_user_dealloc(struct VM *vm, void *va, size_t n)
{
  uint8_t *a, *end;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((a > end) || (end > (uint8_t *) KERNEL_BASE))
    panic("invalid range [%p,%p)", a, end);

  for ( ; a < end; a += PAGE_SIZE)
    vm_page_remove(vm->trtab, a);
}

/*
 * ----------------------------------------------------------------------------
 * Copying Data Between Address Spaces
 * ----------------------------------------------------------------------------
 */

int
vm_user_copy_out(struct VM *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *src = (uint8_t *) src_va;
  uint8_t *dst = (uint8_t *) dst_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_page_lookup(vm->trtab, dst, NULL)) == NULL)
      return -EFAULT;
    
    kva    = (uint8_t *) page2kva(page);
    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    memmove(kva + offset, src, ncopy);

    src += ncopy;
    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

int
vm_user_copy_in(struct VM *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *dst = (uint8_t *) dst_va;
  uint8_t *src = (uint8_t *) src_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_page_lookup(vm->trtab, src, NULL)) == NULL)
      return -EFAULT;

    kva    = (uint8_t *) page2kva(page);
    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    memmove(dst, kva + offset, ncopy);

    src += ncopy;
    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * Check User Memory Permissions
 * ----------------------------------------------------------------------------
 */

int
vm_user_check_buf(struct VM *vm, const void *va, size_t n, unsigned perm)
{
  struct Page *page, *new_page;
  const char *p, *end;
  l2_desc_t *pte;

  p   = ROUND_DOWN((const char *) va, PAGE_SIZE);
  end = ROUND_UP((const char *) va + n, PAGE_SIZE);

  if ((p >= (char *) KERNEL_BASE) || (end > (char *) KERNEL_BASE))
    return -EFAULT;

  while (p != end) {
    unsigned curr_perm;

    if ((page = vm_page_lookup(vm->trtab, (void *) p, &pte)) == NULL)
      return -EFAULT;

    curr_perm = mmu_pte_get_flags(pte);

    if ((perm & VM_WRITE) && (curr_perm & VM_COW)) {
      curr_perm &= ~VM_COW;
      curr_perm |= VM_WRITE;

      if ((curr_perm & perm) != perm)
        return -EFAULT;

      if ((new_page = page_alloc_one(0)) == NULL)
        return -EFAULT;

      memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);

      if ((vm_page_insert(vm->trtab, new_page, (void *) p, curr_perm)) != 0)
        return -EFAULT;
    } else {
      if ((curr_perm & perm) != perm)
        return -EFAULT;
    }

    p += PAGE_SIZE;
  }

  return 0;
}

int
vm_user_check_str(struct VM *vm, const char *s, unsigned perm)
{
  struct Page *page;
  const char *p;
  unsigned off;
  l2_desc_t *pte;

  assert(KERNEL_BASE % PAGE_SIZE == 0);

  while (s < (char *) KERNEL_BASE) {
    page = vm_page_lookup(vm->trtab, s, &pte);
    if ((page == NULL) || ((mmu_pte_get_flags(pte) & perm) != perm))
      return -EFAULT;

    p = (const char *) page2kva(page);
    for (off = (uintptr_t) s % PAGE_SIZE; off < PAGE_SIZE; off++)
      if (p[off] == '\0')
        return 0;
    s += off;
  }

  return -EFAULT;
}

/*
 * ----------------------------------------------------------------------------
 * Loading Binaries
 * ----------------------------------------------------------------------------
 */

int
vm_user_load(struct VM *vm, void *va, struct Inode *ip, size_t n, off_t off)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    page = vm_page_lookup(vm->trtab, dst, NULL);
    if (page == NULL)
      return -EFAULT;

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) dst % PAGE_SIZE;
    ncopy  = MIN(PAGE_SIZE - offset, n);

    if ((r = fs_inode_read(ip, kva + offset, ncopy, &off)) != ncopy)
      return r;

    dst += ncopy;
    n   -= ncopy;
  }

  return 0;
}

struct VM   *
vm_create(void)
{
  struct VM *vm;

  if ((vm = (struct VM *) kobject_alloc(vm_pool)) == NULL)
    return NULL;

  if ((vm->trtab = mmu_pgtab_create()) == NULL) {
    kobject_free(vm_pool, vm);
    return NULL;
  }

  list_init(&vm->areas);

  return vm;
}

void
vm_destroy(struct VM *vm)
{
  vm_user_dealloc(vm, (void *) 0, ROUND_UP(vm->heap, PAGE_SIZE));
  vm_user_dealloc(vm, (void *) vm->stack, KERNEL_BASE - vm->stack);

  mmu_pgtab_destroy(vm->trtab);

  kobject_free(vm_pool, vm);
}

struct VM   *
vm_clone(struct VM *vm)
{
  struct VM *new_vm;
  struct Page *src_page, *dst_page;
  uint8_t *va;
  l2_desc_t *pte;

  if ((new_vm = vm_create()) == NULL)
    return NULL;

  va = (uint8_t *) 0;

  while (va < (uint8_t *) KERNEL_BASE) {
    src_page = vm_page_lookup(vm->trtab, va, &pte);

    if (src_page != NULL) {
      unsigned perm;

      perm = mmu_pte_get_flags(pte);

      if ((perm & VM_WRITE) || (perm & VM_COW)) {
        perm &= ~VM_WRITE;
        perm |= VM_COW;

        if (vm_page_insert(vm->trtab, src_page, va, perm) < 0)
          panic("Cannot change page permissions");

        if (vm_page_insert(new_vm->trtab, src_page, va, perm) < 0) {
          vm_destroy(new_vm);
          return NULL;
        }
      } else {
        if ((dst_page = page_alloc_one(0)) == NULL) {
          vm_destroy(new_vm);
          return NULL;
        }

        if (vm_page_insert(new_vm->trtab, dst_page, va, perm) < 0) {
          page_free_one(dst_page);
          vm_destroy(new_vm);
          return NULL;
        }

        memcpy(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
      }
    }

    va += PAGE_SIZE;
  }

  return new_vm;
}

void
vm_init(void)
{
  vm_pool = kobject_pool_create("vm_pool", sizeof(struct VM), 0);
}

int
vm_handle_fault(struct VM *vm, uintptr_t va)
{
  struct Page *fault_page, *page;
  l2_desc_t *pte;

  fault_page = vm_page_lookup(vm->trtab, (void *) va, &pte);

  if (fault_page == NULL) {
    if ((va < vm->stack) &&
        (va >= (vm->stack - PAGE_SIZE)) &&
        (vm->heap < (vm->stack - PAGE_SIZE))) {
      // Expand stack
      if (vm_user_alloc(vm, (void *) (vm->stack - PAGE_SIZE),
          PAGE_SIZE, VM_WRITE | VM_USER) == 0) {
        vm->stack -= PAGE_SIZE;
        return 0;
      }
    }
  } else {
    int prot;

    prot = mmu_pte_get_flags(pte);
    if ((prot & VM_COW) && ((page = page_alloc_one(0)) != NULL)) {
      memcpy(page2kva(page), page2kva(fault_page), PAGE_SIZE);

      prot &= ~VM_COW;
      prot |= VM_WRITE;

      if (vm_page_insert(vm->trtab, page, (void *) va, prot) == 0)
        return 0;
    }
  }

  return -EFAULT;
}
