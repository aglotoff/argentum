#include <assert.h>
#include <errno.h>
#include <string.h>

#include "armv7.h"
#include "console.h"
#include "kernel.h"
#include "mmu.h"
#include "page.h"
#include "vm.h"

tte_t *kern_trtab;

static pte_t *vm_walk_trtab(tte_t *, uintptr_t, int);
static void vm_map_region(tte_t *, uintptr_t, uint32_t, size_t, int, int);

void
vm_init(void)
{
  extern uint8_t _start[];

  struct PageInfo *p;

  // Create the master translation table
  if ((p = page_alloc_block(2, PAGE_ALLOC_ZERO)) == NULL)
    panic("out of memory");

  kern_trtab = (tte_t *) page2kva(p);

  // Map all of physical memory at KERNEL_BASE
  // Permissions: kernel RW, user NONE
  vm_map_region(kern_trtab, KERNEL_BASE, 0, PHYS_TOP, AP_PRIV_RW, 0);

  // Map exception vectors at VECTORS_BASE
  // Permissions: kernel R, user NONE
  vm_map_region(kern_trtab, VECTORS_BASE, (uint32_t) _start,
                PAGE_SIZE, AP_PRIV_RO, 0);

  vm_init_percpu();
}

void
vm_switch_user(tte_t *trtab)
{
  write_ttbr0(PADDR(trtab));
  tlbiall();
}

void
vm_switch_kernel(void)
{
  write_ttbr0(PADDR(kern_trtab));
  tlbiall();
}

void
vm_init_percpu(void)
{
  // Switch from the minimal entry translation table to the full translation
  // table.
  write_ttbr0(PADDR(kern_trtab));
  write_ttbr1(PADDR(kern_trtab));

  // Size of the TTBR0 translation table is 8KB.
  write_ttbcr(1);

  tlbiall();
}

static pte_t *
vm_walk_trtab(tte_t *trtab, uintptr_t va, int alloc)
{
  struct PageInfo *p;
  tte_t *tte;
  pte_t *pgtab;
  unsigned i;

  tte = &trtab[TTX(va)];
  if ((*tte & TTE_TYPE_MASK) == TTE_TYPE_FAULT) {
    if (!alloc)
      return NULL;

    if ((p = page_alloc(PAGE_ALLOC_ZERO)) == NULL)
      return NULL;
    
    p->ref_count++;

    for (i = 0; i < 4; i++)
      trtab[(TTX(va) & ~3)+i] = (page2pa(p) + (PGTAB_SIZE*i)) | TTE_TYPE_PGTAB;
  } else if ((*tte & TTE_TYPE_MASK) != TTE_TYPE_PGTAB) {
    warn("tte type is not page table");
    return NULL;
  }

  pgtab = KADDR(TTE_PGTAB_ADDR(*tte));
  return &pgtab[PTX(va)];
}

void *
vm_map_mmio(uint32_t pa, size_t n)
{
  static uint8_t *base = (uint8_t *) MMIO_BASE;
  void *ret;

  ret = base;
  base += n;

  if (base > (uint8_t *) MMIO_LIMIT)
    panic("out of memory");

  vm_map_region(kern_trtab, (uintptr_t) ret, pa, n, AP_PRIV_RW, 1);
  return ret;
}

struct PageInfo *
vm_lookup_page(tte_t *trtab, void *va, pte_t **pte_store)
{
  pte_t *pte;

  if ((pte = vm_walk_trtab(trtab, (uintptr_t ) va, 0)) == NULL)
    return NULL;
  if ((*pte & PTE_TYPE_MASK) != PTE_TYPE_SMALL)
    return NULL;

  if (pte_store)
    *pte_store = pte;
  
  return pa2page(PTE_SMALL_ADDR(*pte));
}

int
vm_insert_page(tte_t *trtab, struct PageInfo *p, void *va, int perm)
{
  pte_t *pte;

  if ((pte = vm_walk_trtab(trtab, (uintptr_t ) va, 1)) == NULL)
    return -ENOMEM;

  p->ref_count++;
  vm_remove_page(trtab, va);
  *pte = page2pa(p) | PTE_AP(perm) | PTE_TYPE_SMALL;

  return 0;
}

void
vm_remove_page(tte_t *trtab, void *va)
{
  struct PageInfo *page;
  pte_t *pte;

  if ((page = vm_lookup_page(trtab, va, &pte)) == NULL)
    return;

  if (--page->ref_count == 0)
    page_free(page);

  *pte = 0;
}

int
vm_alloc_region(tte_t *trtab, void *va, size_t n)
{
  struct PageInfo *page;
  uintptr_t a, start, end;

  start = (uintptr_t) va & ~(PAGE_SIZE - 1);
  end   = ((uintptr_t) va + n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  for (a = start; a < end; a += PAGE_SIZE) {
    if ((page = page_alloc(PAGE_ALLOC_ZERO)) == NULL) {
      vm_dealloc_region(trtab, (void *) start, a - start);
      return -ENOMEM;
    }

    if ((vm_insert_page(trtab, page, (void *) a, AP_BOTH_RW)) != 0) {
      vm_dealloc_region(trtab, (void *) start, a - start);
      page_free(page);
      return -ENOMEM;
    }
  }

  return 0;
}

void
vm_dealloc_region(tte_t *trtab, void *va, size_t n)
{
  uintptr_t a, start, end;

  start = (uintptr_t) va & ~(PAGE_SIZE - 1);
  end   = ((uintptr_t) va + n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  for (a = start; a < end; a += PAGE_SIZE)
    vm_remove_page(trtab, (void *) a);
}

int
vm_copy_out(tte_t *trtab, void *dst, const void *src, size_t n)
{
  struct PageInfo *page;
  uint8_t *s, *d, *kva;
  size_t ncopied, offset;

  s = (uint8_t *) src;
  d = (uint8_t *) dst;

  while (n != 0) {
    page = vm_lookup_page(trtab, d, NULL);
    if (page == NULL)
      return -EFAULT;

    kva = (uint8_t *) page2kva(page);

    offset = (uintptr_t) d % PAGE_SIZE;
    ncopied = MIN(PAGE_SIZE - offset, n);

    memmove(kva + offset, s, ncopied);

    s += ncopied;
    d += ncopied;
    n -= ncopied;
  }

  return 0;
}

int
vm_copy_in(tte_t *trtab, void *dst, const void *src, size_t n)
{
  struct PageInfo *page;
  uint8_t *s, *d, *kva;
  size_t ncopied, offset;

  s = (uint8_t *) src;
  d = (uint8_t *) dst;

  while (n != 0) {
    page = vm_lookup_page(trtab, s, NULL);
    if (page == NULL)
      return -EFAULT;

    kva = (uint8_t *) page2kva(page);

    offset  = (uintptr_t) d % PAGE_SIZE;
    ncopied = MIN(PAGE_SIZE - offset, n);

    memmove(d, kva + offset, ncopied);

    s += ncopied;
    d += ncopied;
    n -= ncopied;
  }

  return 0;
}

// 
// Map [va, va + n) of virtual address space to physical [pa, pa + n) in
// the translation page trtab.
//
// This function is only intended to set up static mappings in the kernel's
// portion of address space.
//
static void
vm_map_region(tte_t *trtab,
              uintptr_t va, uint32_t pa, size_t n,
              int perm, int dev)
{
  pte_t *pte;
  tte_t *tte;

  assert(va % PAGE_SIZE == 0);
  assert(pa % PAGE_SIZE == 0);
  assert(n % PAGE_SIZE == 0);

  while (n != 0) {
    // Whenever possible, map entire 1MB sections to reduce the memory
    // management overhead.
    if ((va % PAGE_SECT_SIZE == 0) && (pa % PAGE_SECT_SIZE == 0) &&
        (n % PAGE_SECT_SIZE == 0)) {
      tte = &trtab[TTX(va)];
      if (*tte)
        panic("remap");

      *tte = pa | TTE_SECT_AP(perm) | TTE_TYPE_SECT;
      if (!dev)
        *tte |= (TTE_SECT_C | TTE_SECT_B);

      va += PAGE_SECT_SIZE;
      pa += PAGE_SECT_SIZE;
      n -= PAGE_SECT_SIZE;
    } else {
      if ((pte = vm_walk_trtab(trtab, va, 1)) == NULL)
        panic("out of memory");

      if (*pte)
        panic("remap %p", *pte);

      *pte = pa | PTE_AP(perm) | PTE_TYPE_SMALL;
      if (!dev)
        *pte |= (PTE_C | PTE_B);

      va += PAGE_SIZE;
      pa += PAGE_SIZE;
      n -= PAGE_SIZE;
    }
  }
}
