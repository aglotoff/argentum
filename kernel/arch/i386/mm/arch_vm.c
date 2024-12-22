#include <sys/mman.h>

#include <kernel/mm/memlayout.h>
#include <kernel/vm.h>
#include <kernel/page.h>

void
arch_vm_load(void *pgtab)
{
  // TODO
  (void) pgtab;
}

void
arch_vm_load_kernel(void)
{
  // TODO
}

int
arch_vm_pte_valid(void *pte)
{
  // TODO
  (void) pte;
  return -1;
}

physaddr_t
arch_vm_pte_addr(void *pte)
{
  // TODO
  (void) pte;
  return 0;
}

int
arch_vm_pte_flags(void *pte)
{
  // TODO
  (void) pte;
  return -1;
}

void
arch_vm_pte_set(void *pte, physaddr_t pa, int flags)
{
  // TODO
  (void) pte;
  (void) pa;
  (void) flags;
}

void
arch_vm_pte_clear(void *pte)
{
  // TODO
  (void) pte;
}

void
arch_vm_invalidate(uintptr_t va)
{
  // TODO
  (void) va;
}

void *
arch_vm_lookup(void *pgtab, uintptr_t va, int alloc)
{
  // TODO
  (void) pgtab;
  (void) va;
  (void) alloc;
  return NULL;
}

void
arch_vm_init(void)
{
  // TODO
}

void
arch_vm_init_percpu(void)
{
  // TODO
}

void *
arch_vm_create(void)
{
  // TODO
  return NULL;
}

void
arch_vm_destroy(void *pgtab)
{
  // TODO
  (void) pgtab;
}
