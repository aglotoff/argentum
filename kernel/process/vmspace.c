#include <kernel/core/assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include <kernel/console.h>
#include <kernel/tty.h>
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

/*
 * ----------------------------------------------------------------------------
 * Loading Binaries
 * ----------------------------------------------------------------------------
 */

struct VMSpace   *
vm_space_create(void)
{
  struct VMSpace *vm;

  if ((vm = (struct VMSpace *) k_object_pool_get(vmcache)) == NULL)
    return NULL;

  if ((vm->pgtab = vm_create()) == NULL) {
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

  // vm_user_free(vm->pgtab, 0, ROUND_UP(vm->heap, PAGE_SIZE));
  // vm_user_free(vm->pgtab, vm->stack, USTACK_SIZE);
  
  while (!k_list_is_empty(&vm->areas)) {
    area = K_CONTAINER_OF(vm->areas.next, struct VMSpaceMapEntry, link);
    vm_user_free(vm->pgtab, area->start, area->length);

    k_list_remove(&area->link);
    k_object_pool_put(vm_areacache, area);
  }

  vm_destroy(vm->pgtab);

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

  K_LIST_FOREACH(&vm->areas, l) {
    area = K_CONTAINER_OF(l, struct VMSpaceMapEntry, link);

    new_area = (struct VMSpaceMapEntry *) k_object_pool_get(vm_areacache);
    if (new_area == NULL) {
      vm_space_destroy(new_vm);
      return NULL;
    }

    new_area->start  = area->start;
    new_area->length = area->length;
    new_area->flags  = area->flags;
    k_list_add_back(&new_vm->areas, &new_area->link);

    if (vm_user_clone(vm->pgtab, new_vm->pgtab, area->start, area->length, share) < 0) {
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


intptr_t
vm_space_map(struct VMSpace *vm, uintptr_t addr, size_t n, int flags)
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
    area = K_CONTAINER_OF(l, struct VMSpaceMapEntry, link);

    // Can insert before
    if ((va + n) <= area->start)
      break;

    if (va < (area->start + area->length))
      va = area->start + area->length;
  }

  if ((va + n) > VIRT_KERNEL_BASE)
    return -ENOMEM;

  if ((r = vm_user_alloc(vm->pgtab, va, n, flags)) < 0) {
    vm_user_free(vm->pgtab, va, n);
    return r;
  }

  // Can merge with previous?
  prev = NULL;
  if (l->prev != &vm->areas) {
    prev = K_CONTAINER_OF(l->prev, struct VMSpaceMapEntry, link);
    if (((prev->start + prev->length) != va) || (prev->flags != flags))
      prev = NULL;
  }

  // Can merge with next?
  next = NULL;
  if (l != &vm->areas) {
    next = K_CONTAINER_OF(l, struct VMSpaceMapEntry, link);
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
      vm_user_free(vm->pgtab, va, n);
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
vm_space_dump(struct VMSpace *vm)
{
  struct KListLink *l;
  struct VMSpaceMapEntry *area;
  
  cprintf("vm:\n");
  K_LIST_FOREACH(&vm->areas, l) {
    area = K_CONTAINER_OF(l, struct VMSpaceMapEntry, link);
    cprintf("  [%x-%x)\n", area->start, area->start + area->length);
  }
}

static int
vm_space_copy_out_callback(void *arg, void *kva, size_t n)
{
  uint8_t **src = (uint8_t **) arg;
  memmove(kva, *src, n);
  *src += n;
  return n;
}

int
vm_space_copy_out(struct VMSpace *vm, const void *src, uintptr_t dst_va, size_t n)
{
  uint8_t *p = (uint8_t *) src;

  return vm_user_iterate(vm->pgtab, dst_va, n, vm_space_copy_out_callback, &p, VM_WRITE);
}

static int
vm_space_copy_in_callback(void *arg, void *kva, size_t n)
{
  uint8_t **dst = (uint8_t **) arg;
  memmove(*dst, kva, n);
  *dst += n;
  return n;
}

int
vm_space_copy_in(struct VMSpace *vm, void *dst, uintptr_t src_va, size_t n)
{
  uint8_t *p = (uint8_t *) dst;

  return vm_user_iterate(vm->pgtab, src_va, n, vm_space_copy_in_callback, &p, VM_READ);
}

struct copy_in_string {
  char *buf;
  size_t i;
  size_t max_len;
};

static int
vm_space_copy_in_string_callback(void *arg, void *kva, size_t n)
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
vm_space_copy_in_string(struct VMSpace *vm, char *dst, uintptr_t src_va, size_t max_len)
{
  struct copy_in_string ctx = { dst, 0, max_len };
  int r;

  r = vm_user_iterate(vm->pgtab, src_va, max_len, vm_space_copy_in_string_callback, &ctx, VM_READ);
  if (r < 0)
    return r;

  return ctx.i;
}
