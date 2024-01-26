#include <kernel/assert.h>
#include <errno.h>
#include <string.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/console.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>
#include <kernel/object_pool.h>
#include <kernel/mm/mmu.h>
#include <kernel/mm/page.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>

static struct ObjectPool *vmcache;
static struct ObjectPool *vm_areacache;

int
vm_range_alloc(struct VMSpace *vm, uintptr_t va, size_t n, int prot)
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
      vm_range_free(vm, (uintptr_t) start, a - start);
      return -ENOMEM;
    }

    if ((r = (vm_page_insert(vm->pgdir, page, (uintptr_t) a, prot)) != 0)) {
      page_free_one(page);
      vm_range_free(vm, (uintptr_t) start, a - start);
      return r;
    }
  }

  return 0;
}

void
vm_range_free(struct VMSpace *vm, uintptr_t va, size_t n)
{
  uint8_t *a, *end;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  if ((a > end) || (end > (uint8_t *) VIRT_KERNEL_BASE))
    panic("invalid range [%p,%p)", a, end);

  for ( ; a < end; a += PAGE_SIZE)
    vm_page_remove(vm->pgdir, (uintptr_t) a);
}

int
vm_range_clone(struct VMSpace *src, struct VMSpace *dst, uintptr_t va, size_t n, int share)
{
  struct Page *src_page, *dst_page;
  uint8_t *a, *end;
  int r;

  a   = ROUND_DOWN((uint8_t *) va, PAGE_SIZE);
  end = ROUND_UP((uint8_t *) va + n, PAGE_SIZE);

  for ( ; a < end; a += PAGE_SIZE) {
    int perm;

    src_page = vm_page_lookup(src->pgdir, (uintptr_t) a, &perm);

    if (src_page != NULL) {
      if (share) {
        if (perm & VM_COW) {
          if ((dst_page = page_alloc_one(0)) == NULL)
            return -ENOMEM;

          perm |= VM_WRITE;
          perm &= ~VM_COW;

          memmove(page2kva(dst_page), page2kva(src_page), PAGE_SIZE);

          if ((r = vm_page_insert(src->pgdir, dst_page, (uintptr_t) va, perm)) < 0) {
            page_free_one(dst_page);
            return r;
          }

          if ((r = vm_page_insert(dst->pgdir, dst_page, (uintptr_t) va, perm)) < 0) {
            return r;
          }
        } else {
          if ((r = vm_page_insert(dst->pgdir, src_page, (uintptr_t) a, perm)) < 0)
            return r;
        }
      } else if ((perm & VM_WRITE) || (perm & VM_COW)) {
        perm &= ~VM_WRITE;
        perm |= VM_COW;

        if ((r = vm_page_insert(src->pgdir, src_page, (uintptr_t) a, perm)) < 0)
          return r;
        if ((r = vm_page_insert(dst->pgdir, src_page, (uintptr_t) a, perm)) < 0)
          return r;
      } else {
        if ((dst_page = page_alloc_one(0)) == NULL)
          return -ENOMEM;

        if ((r = vm_page_insert(dst->pgdir, dst_page, (uintptr_t) va, perm)) < 0) {
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
vm_space_copy_out(struct VMSpace *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *src = (uint8_t *) src_va;
  uint8_t *dst = (uint8_t *) dst_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_page_lookup(vm->pgdir, (uintptr_t) dst, NULL)) == NULL)
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
vm_space_copy_in(struct VMSpace *vm, void *dst_va, const void *src_va, size_t n)
{
  uint8_t *dst = (uint8_t *) dst_va;
  uint8_t *src = (uint8_t *) src_va;

  while (n != 0) {
    struct Page *page;
    uint8_t *kva;
    size_t offset, ncopy;

    if ((page = vm_page_lookup(vm->pgdir, (uintptr_t) src, NULL)) == NULL)
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
vm_space_check_buf(struct VMSpace *vm, const void *va, size_t n, unsigned perm)
{
  struct Page *page, *new_page;
  const char *p, *end;

  p   = ROUND_DOWN((const char *) va, PAGE_SIZE);
  end = ROUND_UP((const char *) va + n, PAGE_SIZE);

  if ((p >= (char *) VIRT_KERNEL_BASE) || (end > (char *) VIRT_KERNEL_BASE))
    return -EFAULT;

  while (p != end) {
    int curr_perm;

    if ((page = vm_page_lookup(vm->pgdir, (uintptr_t) p, &curr_perm)) == NULL)
      return -EFAULT;

    if ((perm & VM_WRITE) && (curr_perm & VM_COW)) {
      curr_perm &= ~VM_COW;
      curr_perm |= VM_WRITE;

      if ((curr_perm & perm) != perm)
        return -EFAULT;

      if ((new_page = page_alloc_one(0)) == NULL)
        return -EFAULT;

      memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);

      if ((vm_page_insert(vm->pgdir, new_page, (uintptr_t) p, curr_perm)) != 0)
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
vm_space_check_str(struct VMSpace *vm, const char *s, unsigned perm)
{
  struct Page *page;
  const char *p;
  unsigned off;

  assert(VIRT_KERNEL_BASE % PAGE_SIZE == 0);

  while (s < (char *) VIRT_KERNEL_BASE) {
    int flags;

    page = vm_page_lookup(vm->pgdir, (uintptr_t) s, &flags);
    if ((page == NULL) || ((flags & perm) != perm))
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
vm_space_load_inode(struct VMSpace *vm, void *va, struct Inode *ip, size_t n, off_t off)
{
  struct Page *page;
  uint8_t *dst, *kva;
  int ncopy, offset;
  int r;

  dst = (uint8_t *) va;

  while (n != 0) {
    page = vm_page_lookup(vm->pgdir, (uintptr_t) dst, NULL);
    if (page == NULL) {
      return -EFAULT;
    }

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

struct VMSpace   *
vm_space_create(void)
{
  struct VMSpace *vm;

  if ((vm = (struct VMSpace *) object_pool_get(vmcache)) == NULL)
    return NULL;

  if ((vm->pgdir = vm_create()) == NULL) {
    object_pool_put(vmcache, vm);
    return NULL;
  }

  // list_init(&vm->areas);
  vm->heap  = 0;
  vm->stack = 0;

  return vm;
}

void
vm_space_destroy(struct VMSpace *vm)
{
  // struct VMSpaceMapEntry *area;

  vm_range_free(vm, 0, ROUND_UP(vm->heap, PAGE_SIZE));
  vm_range_free(vm, vm->stack, USTACK_SIZE);
  
  // while (!list_empty(&vm->areas)) {
  //   area = LIST_CONTAINER(vm->areas.next, struct VMSpaceMapEntry, link);
  //   vm_range_free(vm, area->start, area->length);
  //   list_remove(&area->link);
  // }

  vm_destroy(vm->pgdir);

  object_pool_put(vmcache, vm);
}

struct VMSpace   *
vm_space_clone(struct VMSpace *vm, int share)
{
  struct VMSpace *new_vm;
  // struct ListLink *l;
  // struct VMSpaceMapEntry *area, *new_area;

  if ((new_vm = vm_space_create()) == NULL)
    return NULL;

  if (vm_range_clone(vm, new_vm, 0, ROUND_UP(vm->heap, PAGE_SIZE), share) < 0) {
    vm_space_destroy(new_vm);
    return NULL;
  }
  new_vm->heap = vm->heap;

  if (vm_range_clone(vm, new_vm, vm->stack, USTACK_SIZE, 0) < 0) {
    vm_space_destroy(new_vm);
    return NULL;
  }
  new_vm->stack = vm->stack;

  // LIST_FOREACH(&vm->areas, l) {
  //   area = LIST_CONTAINER(l, struct VMSpaceMapEntry, link);

  //   new_area = (struct VMSpaceMapEntry *) object_pool_get(vm_areacache);
  //   if (new_area == NULL) {
  //     vm_space_destroy(new_vm);
  //     return NULL;
  //   }

  //   new_area->start  = area->start;
  //   new_area->length = area->length;
  //   new_area->flags  = area->flags;
  //   list_add_front(&new_vm->areas, &new_area->link);

  //   if (vm_range_clone(vm, new_vm, area->start, area->length) < 0) {
  //     vm_space_destroy(new_vm);
  //     return NULL;
  //   }
  // }

  return new_vm;
}

void
vm_space_init(void)
{
  vmcache = object_pool_create("vmcache", sizeof(struct VMSpace), 0, 0, NULL, NULL);
  vm_areacache = object_pool_create("vm_areacache", sizeof(struct VMSpaceMapEntry), 0, 0, NULL, NULL);
}

int
vm_handle_fault(struct VMSpace *vm, uintptr_t va)
{
  struct Page *fault_page, *page;
  int flags;

  if (va < PAGE_SIZE || va >= VIRT_KERNEL_BASE)
    return -EFAULT;

  if (va >= vm->heap && va < vm->stack)
    return -EFAULT;

  fault_page = vm_page_lookup(vm->pgdir, va, &flags);

  if ((flags & VM_COW) && ((page = page_alloc_one(0)) != NULL)) {
    memcpy(page2kva(page), page2kva(fault_page), PAGE_SIZE);

    if (vm_page_insert(vm->pgdir, page, va, (flags & ~VM_COW) | VM_WRITE) == 0)
      return 0;
  }

  return -EFAULT;
}

// intptr_t
// vm_space_alloc(struct VMSpace *vm, uintptr_t addr, size_t n, int flags)
// {
//   uintptr_t va;
//   struct ListLink *l;
//   struct VMSpaceMapEntry *area, *prev, *next;
//   int r;

//   va = addr ? ROUND_UP((uintptr_t) addr, PAGE_SIZE) : PAGE_SIZE;
//   n  = ROUND_UP(n, PAGE_SIZE);

//   if ((va >= VIRT_KERNEL_BASE) || ((va + n) > VIRT_KERNEL_BASE) || ((va + n) <= va))
//     return -EINVAL;

//   // Find Vm area to insert before
//   for (l = vm->areas.next; l != &vm->areas; l = l->next) {
//     area = LIST_CONTAINER(l, struct VMSpaceMapEntry, link);

//     // Can insert before
//     if ((va + n) <= area->start)
//       break;

//     if (va < (area->start + area->length))
//       va = area->start + area->length;
//   }

//   if ((va + n) > VIRT_KERNEL_BASE)
//     return -ENOMEM;

//   if ((r = vm_range_alloc(vm, va, n, flags)) < 0) {
//     vm_range_free(vm, va, n);
//     return r;
//   }

//   // Can merge with previous?
//   prev = NULL;
//   if (l->prev != &vm->areas) {
//     prev = LIST_CONTAINER(l->prev, struct VMSpaceMapEntry, link);
//     if (((prev->start + prev->length) != va) || (prev->flags != flags))
//       prev = NULL;
//   }

//   // Can merge with next?
//   next = NULL;
//   if (l != &vm->areas) {
//     next = LIST_CONTAINER(l, struct VMSpaceMapEntry, link);
//     if ((next->start != (va + n)) || (next->flags != flags))
//       next = NULL;
//   }

//   if ((prev != NULL) && (next != NULL)) {
//     prev->length += next->length + n;

//     list_remove(&next->link);
//     object_pool_put(vm_areacache, next);
//   } else if (prev != NULL) {
//     prev->length += n;
//   } else if (next != NULL) {
//     next->start = va;
//   } else {
//     area = (struct VMSpaceMapEntry *) object_pool_get(vm_areacache);
//     if (area == NULL) {
//       vm_range_free(vm, va, n);
//       return -ENOMEM;
//     }

//     area->start  = va;
//     area->length = n;
//     area->flags  = flags;

//     list_add_back(l, &area->link);
//   }

//   // cprintf("[pages_free %d]\n", pages_free);

//   return va;
// }

// void
// vm_print_areas(struct VMSpace *vm)
// {
//   struct ListLink *l;
//   struct VMSpaceMapEntry *area;
  
//   cprintf("vm:\n");
//   LIST_FOREACH(&vm->areas, l) {
//     area = LIST_CONTAINER(l, struct VMSpaceMapEntry, link);
//     cprintf("  [%x-%x)\n", area->start, area->start + area->length);
//   }
// }
