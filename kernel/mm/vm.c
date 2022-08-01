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
static struct KObjectPool *vm_area_pool;

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

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
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

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
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

  if ((uintptr_t) va >= VIRT_KERNEL_BASE)
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
vm_range_alloc(struct VM *vm, void *va, size_t n, int prot)
{
  struct Page *page;
  uint8_t *a, *start, *end;
  int r;

  start = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end   = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((start > end) || (end > (uint8_t *) VIRT_KERNEL_BASE))
    return -EINVAL;

  for (a = start; a < end; a += PAGE_SIZE) {
    if ((page = page_alloc_one(PAGE_ALLOC_ZERO)) == NULL) {
      vm_range_free(vm, start, a - start);
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm->trtab, page, a, prot)) != 0)) {
      page_free_one(page);
      vm_range_free(vm, start, a - start);
      return r;
    }
  }

  return 0;
}

void
vm_range_free(struct VM *vm, void *va, size_t n)
{
  uint8_t *a, *end;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((a > end) || (end > (uint8_t *) VIRT_KERNEL_BASE))
    panic("invalid range [%p,%p)", a, end);

  for ( ; a < end; a += PAGE_SIZE)
    vm_page_remove(vm->trtab, a);
}

int
vm_range_clone(struct VM *src, struct VM *dst, void *va, size_t n)
{
  struct Page *src_page, *dst_page;
  l2_desc_t *pte;
  uint8_t *a, *end;
  int r;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  for ( ; a < end; a += PAGE_SIZE) {
    src_page = vm_page_lookup(src->trtab, a, &pte);

    if (src_page != NULL) {
      unsigned perm;

      perm = mmu_pte_get_flags(pte);

      if ((perm & VM_WRITE) || (perm & VM_COW)) {
        perm &= ~VM_WRITE;
        perm |= VM_COW;

        if ((r = vm_page_insert(src->trtab, src_page, a, perm)) < 0)
          return r;
        if ((r = vm_page_insert(dst->trtab, src_page, a, perm)) < 0)
          return r;
      } else {
        if ((dst_page = page_alloc_one(0)) == NULL)
          return -ENOMEM;

        if ((r = vm_page_insert(dst->trtab, dst_page, va, perm)) < 0) {
          page_free_one(dst_page);
          return r;
        }

        memcpy(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);
      }
    }
  }

  return 0;
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

  if ((p >= (char *) VIRT_KERNEL_BASE) || (end > (char *) VIRT_KERNEL_BASE))
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

  assert(VIRT_KERNEL_BASE % PAGE_SIZE == 0);

  while (s < (char *) VIRT_KERNEL_BASE) {
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
  struct VMArea *area;
  
  while (!list_empty(&vm->areas)) {
    area = LIST_CONTAINER(vm->areas.next, struct VMArea, link);
    vm_range_free(vm, (void *) area->start, area->length);
    list_remove(&area->link);
  }

  mmu_pgtab_destroy(vm->trtab);

  kobject_free(vm_pool, vm);
}

struct VM   *
vm_clone(struct VM *vm)
{
  struct VM *new_vm;
  struct ListLink *l;
  struct VMArea *area, *new_area;

  if ((new_vm = vm_create()) == NULL)
    return NULL;

  LIST_FOREACH(&vm->areas, l) {
    area = LIST_CONTAINER(l, struct VMArea, link);

    new_area = (struct VMArea *) kobject_alloc(vm_area_pool);
    if (new_area == NULL) {
      vm_destroy(new_vm);
      return NULL;
    }

    new_area->start  = area->start;
    new_area->length = area->length;
    new_area->flags  = area->flags;
    list_add_front(&new_vm->areas, &new_area->link);

    if (vm_range_clone(vm, new_vm, (void *) area->start, area->length) < 0) {
      vm_destroy(new_vm);
      return NULL;
    }
  }

  return new_vm;
}

void
vm_init(void)
{
  vm_pool = kobject_pool_create("vm_pool", sizeof(struct VM), 0);
  vm_area_pool = kobject_pool_create("vm_area_pool", sizeof(struct VMArea), 0);
}

int
vm_handle_fault(struct VM *vm, uintptr_t va)
{
  struct Page *fault_page, *page;
  l2_desc_t *pte;
  int prot;

  fault_page = vm_page_lookup(vm->trtab, (void *) va, &pte);
  prot = mmu_pte_get_flags(pte);

  if ((prot & VM_COW) && ((page = page_alloc_one(0)) != NULL)) {
    memcpy(page2kva(page), page2kva(fault_page), PAGE_SIZE);

    prot &= ~VM_COW;
    prot |= VM_WRITE;

    if (vm_page_insert(vm->trtab, page, (void *) va, prot) == 0)
      return 0;
  }

  return -EFAULT;
}

void *
vm_mmap(struct VM *vm, void *addr, size_t n, int flags)
{
  uintptr_t va;
  struct ListLink *l;
  struct VMArea *area, *prev, *next;
  int r;

  va = (addr == NULL) ? PAGE_SIZE : ROUND_UP((uintptr_t) addr, PAGE_SIZE);
  n  = ROUND_UP(n, PAGE_SIZE);

  if ((va >= VIRT_KERNEL_BASE) || ((va + n) > VIRT_KERNEL_BASE) || ((va + n) <= va))
    return (void *) -EINVAL;
  
  // Find Vm area to insert before
  for (l = vm->areas.next; l != &vm->areas; l = l->next) {
    area = LIST_CONTAINER(l, struct VMArea, link);

    // Can insert before
    if ((va + n) <= area->start)
      break;

    if (va < (area->start + area->length))
      va = area->start + area->length;
  }

  if ((va + n) > VIRT_KERNEL_BASE)
    return (void *) -ENOMEM;

  if ((r = vm_range_alloc(vm, (void *) va, n, flags)) < 0) {
    vm_range_free(vm, (void *) va, n);
    return (void *) r;
  }

  // Can merge with previous?
  prev = NULL;
  if (l->prev != &vm->areas) {
    prev = LIST_CONTAINER(l->prev, struct VMArea, link);
    if (((prev->start + prev->length) != va) || (prev->flags != flags))
      prev = NULL;
  }

  // Can merge with next?
  next = NULL;
  if (l != &vm->areas) {
    next = LIST_CONTAINER(l, struct VMArea, link);
    if ((next->start != (va + n)) || (next->flags != flags))
      next = NULL;
  }

  if ((prev != NULL) && (next != NULL)) {
    prev->length += next->length + n;

    list_remove(&next->link);
    kobject_free(vm_area_pool, next);
  } else if (prev != NULL) {
    prev->length += n;
  } else if (next != NULL) {
    next->start = va;
  } else {
    area = (struct VMArea *) kobject_alloc(vm_area_pool);
    if (area == NULL) {
      vm_range_free(vm, (void *) va, n);
      return (void *) -ENOMEM;
    }

    area->start  = va;
    area->length = n;
    area->flags  = flags;

    list_add_back(l, &area->link);
  }

  return (void *) va;
}

// void
// vm_print_areas(struct VM *vm)
// {
//   struct ListLink *l;
//   struct VMArea *area;
  
//   cprintf("vm:\n");
//   LIST_FOREACH(&vm->areas, l) {
//     area = LIST_CONTAINER(l, struct VMArea, link);
//     cprintf("  [%x-%x)\n", area->start, area->start + area->length);
//   }
// }
