#include <kernel/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include <kernel/cprintf.h>
#include <kernel/drivers/console.h>
#include <kernel/fs/fs.h>
#include <kernel/types.h>
#include <kernel/object_pool.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>

static struct KObjectPool *vmcache;
static struct KObjectPool *vm_areacache;

/*
 * ----------------------------------------------------------------------------
 * Check User Memory Permissions
 * ----------------------------------------------------------------------------
 */

int
vm_space_check_ptr(struct VMSpace *vm, uintptr_t va, unsigned perm)
{
  int curr_perm;

  if (va >= VIRT_KERNEL_BASE)
    return -EFAULT;
  if (vm_page_lookup(vm->pgtab, va, &curr_perm) == NULL)
    return -EFAULT;
  if ((curr_perm & perm) != perm)
    return -EFAULT;
  return 0;
}

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

    if ((page = vm_page_lookup(vm->pgtab, (uintptr_t) p, &curr_perm)) == NULL)
      return -EFAULT;

    if ((perm & PROT_WRITE) && (curr_perm & _PROT_COW)) {
      curr_perm &= ~_PROT_COW;
      curr_perm |= PROT_WRITE;

      if ((curr_perm & perm) != perm)
        return -EFAULT;

      if ((new_page = page_alloc_one(0)) == NULL)
        return -EFAULT;

      memcpy(page2kva(new_page), page2kva(page), PAGE_SIZE);

      if ((vm_page_insert(vm->pgtab, new_page, (uintptr_t) p, curr_perm)) != 0)
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

    page = vm_page_lookup(vm->pgtab, (uintptr_t) s, &flags);
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
    page = vm_page_lookup(vm->pgtab, (uintptr_t) dst, NULL);
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

  if ((vm = (struct VMSpace *) k_object_pool_get(vmcache)) == NULL)
    return NULL;

  if ((vm->pgtab = vm_arch_create()) == NULL) {
    k_object_pool_put(vmcache, vm);
    return NULL;
  }

  k_spinlock_init(&vm->lock, "vmspace");
  k_list_init(&vm->areas);

  return vm;
}

void
vm_space_destroy(struct VMSpace *vm)
{
  struct VMSpaceMapEntry *area;

  // vm_range_free(vm->pgtab, 0, ROUND_UP(vm->heap, PAGE_SIZE));
  // vm_range_free(vm->pgtab, vm->stack, USTACK_SIZE);
  
  while (!k_list_empty(&vm->areas)) {
    area = KLIST_CONTAINER(vm->areas.next, struct VMSpaceMapEntry, link);
    vm_range_free(vm->pgtab, area->start, area->length);

    k_list_remove(&area->link);
    k_object_pool_put(vm_areacache, area);
  }

  vm_arch_destroy(vm->pgtab);

  k_object_pool_put(vmcache, vm);
}

struct VMSpace   *
vm_space_clone(struct VMSpace *vm, int share)
{
  struct VMSpace *new_vm;
  struct KListLink *l;
  struct VMSpaceMapEntry *area, *new_area;

  if ((new_vm = vm_space_create()) == NULL)
    return NULL;

  KLIST_FOREACH(&vm->areas, l) {
    area = KLIST_CONTAINER(l, struct VMSpaceMapEntry, link);

    new_area = (struct VMSpaceMapEntry *) k_object_pool_get(vm_areacache);
    if (new_area == NULL) {
      vm_space_destroy(new_vm);
      return NULL;
    }

    new_area->start  = area->start;
    new_area->length = area->length;
    new_area->flags  = area->flags;
    k_list_add_back(&new_vm->areas, &new_area->link);

    if (vm_range_clone(vm->pgtab, new_vm->pgtab, area->start, area->length, share) < 0) {
      vm_space_destroy(new_vm);
      return NULL;
    }
  }

  return new_vm;
}

void
vm_space_init(void)
{
  vmcache = k_object_pool_create("vmcache", sizeof(struct VMSpace), 0, NULL, NULL);
  vm_areacache = k_object_pool_create("vm_areacache", sizeof(struct VMSpaceMapEntry), 0, NULL, NULL);
}

int
vm_handle_fault(struct VMSpace *vm, uintptr_t va)
{
  struct Page *fault_page, *page;
  int flags;

  if (va < PAGE_SIZE || va >= VIRT_KERNEL_BASE)
    return -EFAULT;

  // cprintf("%p\n", va);

  fault_page = vm_page_lookup(vm->pgtab, va, &flags);

  if (fault_page == NULL)
    return -EFAULT;

  if ((flags & _PROT_COW) && ((page = page_alloc_one(0)) != NULL)) {
    memcpy(page2kva(page), page2kva(fault_page), PAGE_SIZE);

    if (vm_page_insert(vm->pgtab, page, va, (flags & ~_PROT_COW) | PROT_WRITE) == 0)
      return 0;
  }

  return -EFAULT;
}

intptr_t
vmspace_map(struct VMSpace *vm, uintptr_t addr, size_t n, int flags)
{
  uintptr_t va;
  struct KListLink *l;
  struct VMSpaceMapEntry *area, *prev, *next;
  int r;

  va = addr ? ROUND_UP((uintptr_t) addr, PAGE_SIZE) : PAGE_SIZE;
  n  = ROUND_UP(n, PAGE_SIZE);

  if ((va >= VIRT_KERNEL_BASE) || ((va + n) > VIRT_KERNEL_BASE) || ((va + n) <= va))
    return -EINVAL;

  // Find Vm area to insert before
  for (l = vm->areas.next; l != &vm->areas; l = l->next) {
    area = KLIST_CONTAINER(l, struct VMSpaceMapEntry, link);

    // Can insert before
    if ((va + n) <= area->start)
      break;

    if (va < (area->start + area->length))
      va = area->start + area->length;
  }

  if ((va + n) > VIRT_KERNEL_BASE)
    return -ENOMEM;

  if ((r = vm_range_alloc(vm->pgtab, va, n, flags)) < 0) {
    vm_range_free(vm->pgtab, va, n);
    return r;
  }

  // Can merge with previous?
  prev = NULL;
  if (l->prev != &vm->areas) {
    prev = KLIST_CONTAINER(l->prev, struct VMSpaceMapEntry, link);
    if (((prev->start + prev->length) != va) || (prev->flags != flags))
      prev = NULL;
  }

  // Can merge with next?
  next = NULL;
  if (l != &vm->areas) {
    next = KLIST_CONTAINER(l, struct VMSpaceMapEntry, link);
    if ((next->start != (va + n)) || (next->flags != flags))
      next = NULL;
  }

  if ((prev != NULL) && (next != NULL)) {
    prev->length += next->length + n;

    k_list_remove(&next->link);
    k_object_pool_put(vm_areacache, next);
  } else if (prev != NULL) {
    prev->length += n;
  } else if (next != NULL) {
    next->start = va;
  } else {
    area = (struct VMSpaceMapEntry *) k_object_pool_get(vm_areacache);
    if (area == NULL) {
      vm_range_free(vm->pgtab, va, n);
      return -ENOMEM;
    }

    area->start  = va;
    area->length = n;
    area->flags  = flags;

    k_list_add_back(l, &area->link);
  }

  // cprintf("[page_free_count %d]\n", page_free_count);

  return va;
}

void
vm_print_areas(struct VMSpace *vm)
{
  struct KListLink *l;
  struct VMSpaceMapEntry *area;
  
  cprintf("vm:\n");
  KLIST_FOREACH(&vm->areas, l) {
    area = KLIST_CONTAINER(l, struct VMSpaceMapEntry, link);
    cprintf("  [%x-%x)\n", area->start, area->start + area->length);
  }
}
